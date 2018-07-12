/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported Presentation */

"use strict";

ChromeUtils.import("resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "PivotContext", // jshint ignore:line
  "resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "UtteranceGenerator", // jshint ignore:line
  "resource://gre/modules/accessibility/OutputGenerator.jsm");
ChromeUtils.defineModuleGetter(this, "States", // jshint ignore:line
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "Roles", // jshint ignore:line
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "AndroidEvents", // jshint ignore:line
  "resource://gre/modules/accessibility/Constants.jsm");

var EXPORTED_SYMBOLS = ["Presentation"]; // jshint ignore:line

const EDIT_TEXT_ROLES = new Set([
  Roles.SPINBUTTON, Roles.PASSWORD_TEXT,
  Roles.AUTOCOMPLETE, Roles.ENTRY, Roles.EDITCOMBOBOX]);

class AndroidPresentor {
  constructor() {
    this.type = "Android";
    this.displayedAccessibles = new WeakMap();
  }

  /**
   * The virtual cursor's position changed.
   * @param {PivotContext} aContext the context object for the new pivot
   *   position.
   * @param {int} aReason the reason for the pivot change.
   *   See nsIAccessiblePivot.
   * @param {bool} aIsFromUserInput the pivot change was invoked by the user
   */
  pivotChanged(aPosition, aOldPosition, aReason, aStartOffset, aEndOffset) {
    let context = new PivotContext(
      aPosition, aOldPosition, aStartOffset, aEndOffset);
    if (!context.accessible) {
      return null;
    }

    let androidEvents = [];

    const isExploreByTouch = aReason == Ci.nsIAccessiblePivot.REASON_POINT;

    if (isExploreByTouch) {
      // This isn't really used by TalkBack so this is a half-hearted attempt
      // for now.
      androidEvents.push({eventType: AndroidEvents.VIEW_HOVER_EXIT, text: []});
    }

    if (aReason === Ci.nsIAccessiblePivot.REASON_TEXT) {
      const adjustedText = context.textAndAdjustedOffsets;

      androidEvents.push({
        eventType: AndroidEvents.VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY,
        text: [adjustedText.text],
        fromIndex: adjustedText.startOffset,
        toIndex: adjustedText.endOffset
      });
    } else {
      let info = this._infoFromContext(context);
      let eventType = isExploreByTouch ?
        AndroidEvents.VIEW_HOVER_ENTER :
        AndroidEvents.VIEW_ACCESSIBILITY_FOCUSED;
      androidEvents.push({...info, eventType});
    }

    try {
      context.accessibleForBounds.scrollTo(
        Ci.nsIAccessibleScrollType.SCROLL_TYPE_ANYWHERE);
    } catch (e) {}

    if (context.accessible) {
      this.displayedAccessibles.set(context.accessible.document.window, context);
    }

    return androidEvents;
  }

  focused(aObject) {
    let info = this._infoFromContext(
      new PivotContext(aObject, null, -1, -1, true, false));
    return [{ eventType: AndroidEvents.VIEW_FOCUSED, ...info }];
  }

  /**
   * An object's action has been invoked.
   * @param {nsIAccessible} aObject the object that has been invoked.
   * @param {string} aActionName the name of the action.
   */
  actionInvoked(aObject, aActionName) {
    let state = Utils.getState(aObject);

    // Checkable objects use TalkBack's text derived from the event state,
    // so we don't populate the text here.
    let text = null;
    if (!state.contains(States.CHECKABLE)) {
      text = Utils.localize(UtteranceGenerator.genForAction(aObject,
        aActionName));
    }

    return [{
      eventType: AndroidEvents.VIEW_CLICKED,
      text,
      checked: state.contains(States.CHECKED)
    }];
  }

  /**
   * Text has changed, either by the user or by the system. TODO.
   */
  textChanged(aAccessible, aIsInserted, aStart, aLength, aText, aModifiedText) {
    let androidEvent = {
      eventType: AndroidEvents.VIEW_TEXT_CHANGED,
      text: [aText],
      fromIndex: aStart,
      removedCount: 0,
      addedCount: 0
    };

    if (aIsInserted) {
      androidEvent.addedCount = aLength;
      androidEvent.beforeText =
        aText.substring(0, aStart) + aText.substring(aStart + aLength);
    } else {
      androidEvent.removedCount = aLength;
      androidEvent.beforeText =
        aText.substring(0, aStart) + aModifiedText + aText.substring(aStart);
    }

    return [androidEvent];
  }

  /**
   * Text selection has changed. TODO.
   */
  textSelectionChanged(aText, aStart, aEnd, aOldStart, aOldEnd, aIsFromUserInput) {
    let androidEvents = [];

    if (aIsFromUserInput) {
      let [from, to] = aOldStart < aStart ?
        [aOldStart, aStart] : [aStart, aOldStart];
      androidEvents.push({
        eventType: AndroidEvents.VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY,
        text: [aText],
        fromIndex: from,
        toIndex: to
      });
    } else {
      androidEvents.push({
        eventType: AndroidEvents.VIEW_TEXT_SELECTION_CHANGED,
        text: [aText],
        fromIndex: aStart,
        toIndex: aEnd,
        itemCount: aText.length
      });
    }

    return androidEvents;
  }

  /**
   * Selection has changed.
   * XXX: Implement android event?
   * @param {nsIAccessible} aObject the object that has been selected.
   */
  selectionChanged(aObject) {
    return ["todo.selection-changed"];
  }

  /**
   * Name has changed.
   * XXX: Implement android event?
   * @param {nsIAccessible} aAccessible the object whose value has changed.
   */
  nameChanged(aAccessible) {
    return ["todo.name-changed"];
  }

  /**
   * Value has changed.
   * XXX: Implement android event?
   * @param {nsIAccessible} aAccessible the object whose value has changed.
   */
  valueChanged(aAccessible) {
    return ["todo.value-changed"];
  }

  /**
   * The tab, or the tab's document state has changed.
   * @param {nsIAccessible} aDocObj the tab document accessible that has had its
   *    state changed, or null if the tab has no associated document yet.
   * @param {string} aPageState the state name for the tab, valid states are:
   *    'newtab', 'loading', 'newdoc', 'loaded', 'stopped', and 'reload'.
   */
  tabStateChanged(aDocObj, aPageState) {
    return this.announce(
      UtteranceGenerator.genForTabStateChange(aDocObj, aPageState));
  }

  /**
   * The current tab has changed.
   * @param {PivotContext} aDocContext context object for tab's
   *   document.
   * @param {PivotContext} aVCContext context object for tab's current
   *   virtual cursor position.
   */
  tabSelected(aDocContext, aVCContext) {
    return this.pivotChanged(aVCContext, Ci.nsIAccessiblePivot.REASON_NONE);
  }

  /**
   * The viewport has changed, either a scroll, pan, zoom, or
   *    landscape/portrait toggle.
   * @param {Window} aWindow window of viewport that changed.
   */
  viewportChanged(aWindow) {
    let currentContext = this.displayedAccessibles.get(aWindow);

    let events = [{
      eventType: AndroidEvents.VIEW_SCROLLED,
      scrollX: aWindow.scrollX,
      scrollY: aWindow.scrollY,
      maxScrollX: aWindow.scrollMaxX,
      maxScrollY: aWindow.scrollMaxY,
    }];

    if (currentContext) {
      let currentAcc = currentContext.accessibleForBounds;
      if (Utils.isAliveAndVisible(currentAcc)) {
        events.push({
          eventType: AndroidEvents.WINDOW_CONTENT_CHANGED,
          bounds: Utils.getBounds(currentAcc)
        });
      }
    }

    return events;
  }

  /**
   * Announce something. Typically an app state change.
   */
  announce(aAnnouncement) {
    let localizedAnnouncement = Utils.localize(aAnnouncement).join(" ");
    return [{
      eventType: AndroidEvents.ANNOUNCEMENT,
      text: [localizedAnnouncement],
      addedCount: localizedAnnouncement.length,
      removedCount: 0,
      fromIndex: 0
    }];
  }


  /**
   * User tried to move cursor forward or backward with no success.
   * @param {string} aMoveMethod move method that was used (eg. 'moveNext').
   */
  noMove(aMoveMethod) {
    return [{
      eventType: AndroidEvents.VIEW_ACCESSIBILITY_FOCUSED,
      exitView: aMoveMethod,
      text: [""]
    }];
  }

  /**
   * Announce a live region.
   * @param  {PivotContext} aContext context object for an accessible.
   * @param  {boolean} aIsPolite A politeness level for a live region.
   * @param  {boolean} aIsHide An indicator of hide/remove event.
   * @param  {string} aModifiedText Optional modified text.
   */
  liveRegion(aAccessible, aIsPolite, aIsHide, aModifiedText) {
    let context = !aModifiedText ?
      new PivotContext(aAccessible, null, -1, -1, true, !!aIsHide) : null;
    return this.announce(
      UtteranceGenerator.genForLiveRegion(context, aIsHide, aModifiedText));
  }

  _infoFromContext(aContext) {
    const state = Utils.getState(aContext.accessible);
    const info = {
      bounds: aContext.bounds,
      focusable: state.contains(States.FOCUSABLE),
      focused: state.contains(States.FOCUSED),
      clickable: aContext.accessible.actionCount > 0,
      checkable: state.contains(States.CHECKABLE),
      checked: state.contains(States.CHECKED),
      editable: state.contains(States.EDITABLE),
    };

    if (EDIT_TEXT_ROLES.has(aContext.accessible.role)) {
      let textAcc = aContext.accessible.QueryInterface(Ci.nsIAccessibleText);
      return {
        ...info,
        className: "android.widget.EditText",
        hint: aContext.accessible.name,
        text: [textAcc.getText(0, -1)]
      };
    }

    return {
      ...info,
      className: "android.view.View",
      text: Utils.localize(UtteranceGenerator.genForContext(aContext)),
    };
  }
}

const Presentation = new AndroidPresentor();
