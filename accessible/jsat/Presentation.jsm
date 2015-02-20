/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global Components, XPCOMUtils, Utils, Logger, BraillePresenter, Presentation,
          UtteranceGenerator, BrailleGenerator, States, Roles, PivotContext */
/* exported Presentation */

'use strict';

const {utils: Cu, interfaces: Ci} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Logger', // jshint ignore:line
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'PivotContext', // jshint ignore:line
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'UtteranceGenerator', // jshint ignore:line
  'resource://gre/modules/accessibility/OutputGenerator.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'BrailleGenerator', // jshint ignore:line
  'resource://gre/modules/accessibility/OutputGenerator.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Roles', // jshint ignore:line
  'resource://gre/modules/accessibility/Constants.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'States', // jshint ignore:line
  'resource://gre/modules/accessibility/Constants.jsm');

this.EXPORTED_SYMBOLS = ['Presentation']; // jshint ignore:line

/**
 * The interface for all presenter classes. A presenter could be, for example,
 * a speech output module, or a visual cursor indicator.
 */
function Presenter() {}

Presenter.prototype = {
  /**
   * The type of presenter. Used for matching it with the appropriate output method.
   */
  type: 'Base',

  /**
   * The virtual cursor's position changed.
   * @param {PivotContext} aContext the context object for the new pivot
   *   position.
   * @param {int} aReason the reason for the pivot change.
   *   See nsIAccessiblePivot.
   * @param {bool} aIsFromUserInput the pivot change was invoked by the user
   */
  pivotChanged: function pivotChanged(aContext, aReason, aIsFromUserInput) {}, // jshint ignore:line

  /**
   * An object's action has been invoked.
   * @param {nsIAccessible} aObject the object that has been invoked.
   * @param {string} aActionName the name of the action.
   */
  actionInvoked: function actionInvoked(aObject, aActionName) {}, // jshint ignore:line

  /**
   * Text has changed, either by the user or by the system. TODO.
   */
  textChanged: function textChanged(aAccessible, aIsInserted, aStartOffset, // jshint ignore:line
                                    aLength, aText, aModifiedText) {}, // jshint ignore:line

  /**
   * Text selection has changed. TODO.
   */
  textSelectionChanged: function textSelectionChanged(
    aText, aStart, aEnd, aOldStart, aOldEnd, aIsFromUserInput) {}, // jshint ignore:line

  /**
   * Selection has changed. TODO.
   * @param {nsIAccessible} aObject the object that has been selected.
   */
  selectionChanged: function selectionChanged(aObject) {}, // jshint ignore:line

  /**
   * Name has changed.
   * @param {nsIAccessible} aAccessible the object whose value has changed.
   */
  nameChanged: function nameChanged(aAccessible) {}, // jshint ignore: line

  /**
   * Value has changed.
   * @param {nsIAccessible} aAccessible the object whose value has changed.
   */
  valueChanged: function valueChanged(aAccessible) {}, // jshint ignore:line

  /**
   * The tab, or the tab's document state has changed.
   * @param {nsIAccessible} aDocObj the tab document accessible that has had its
   *    state changed, or null if the tab has no associated document yet.
   * @param {string} aPageState the state name for the tab, valid states are:
   *    'newtab', 'loading', 'newdoc', 'loaded', 'stopped', and 'reload'.
   */
  tabStateChanged: function tabStateChanged(aDocObj, aPageState) {}, // jshint ignore:line

  /**
   * The current tab has changed.
   * @param {PivotContext} aDocContext context object for tab's
   *   document.
   * @param {PivotContext} aVCContext context object for tab's current
   *   virtual cursor position.
   */
  tabSelected: function tabSelected(aDocContext, aVCContext) {}, // jshint ignore:line

  /**
   * The viewport has changed, either a scroll, pan, zoom, or
   *    landscape/portrait toggle.
   * @param {Window} aWindow window of viewport that changed.
   */
  viewportChanged: function viewportChanged(aWindow) {}, // jshint ignore:line

  /**
   * We have entered or left text editing mode.
   */
  editingModeChanged: function editingModeChanged(aIsEditing) {}, // jshint ignore:line

  /**
   * Announce something. Typically an app state change.
   */
  announce: function announce(aAnnouncement) {}, // jshint ignore:line


  /**
   * User tried to move cursor forward or backward with no success.
   * @param {string} aMoveMethod move method that was used (eg. 'moveNext').
   */
  noMove: function noMove(aMoveMethod) {},

  /**
   * Announce a live region.
   * @param  {PivotContext} aContext context object for an accessible.
   * @param  {boolean} aIsPolite A politeness level for a live region.
   * @param  {boolean} aIsHide An indicator of hide/remove event.
   * @param  {string} aModifiedText Optional modified text.
   */
  liveRegion: function liveRegionShown(aContext, aIsPolite, aIsHide, // jshint ignore:line
    aModifiedText) {} // jshint ignore:line
};

/**
 * Visual presenter. Draws a box around the virtual cursor's position.
 */
function VisualPresenter() {
  this._displayedAccessibles = new WeakMap();
}

VisualPresenter.prototype = Object.create(Presenter.prototype);

VisualPresenter.prototype.type = 'Visual';

/**
 * The padding in pixels between the object and the highlight border.
 */
VisualPresenter.prototype.BORDER_PADDING = 2;

VisualPresenter.prototype.viewportChanged =
  function VisualPresenter_viewportChanged(aWindow) {
    let currentDisplay = this._displayedAccessibles.get(aWindow);
    if (!currentDisplay) {
      return null;
    }

    let currentAcc = currentDisplay.accessible;
    let start = currentDisplay.startOffset;
    let end = currentDisplay.endOffset;
    if (Utils.isAliveAndVisible(currentAcc)) {
      let bounds = (start === -1 && end === -1) ? Utils.getBounds(currentAcc) :
                   Utils.getTextBounds(currentAcc, start, end);

      return {
        type: this.type,
        details: {
          eventType: 'viewport-change',
          bounds: bounds,
          padding: this.BORDER_PADDING
        }
      };
    }

    return null;
  };

VisualPresenter.prototype.pivotChanged =
  function VisualPresenter_pivotChanged(aContext) {
    if (!aContext.accessible) {
      // XXX: Don't hide because another vc may be using the highlight.
      return null;
    }

    this._displayedAccessibles.set(aContext.accessible.document.window,
                                   { accessible: aContext.accessibleForBounds,
                                     startOffset: aContext.startOffset,
                                     endOffset: aContext.endOffset });

    try {
      aContext.accessibleForBounds.scrollTo(
        Ci.nsIAccessibleScrollType.SCROLL_TYPE_ANYWHERE);

      let bounds = (aContext.startOffset === -1 && aContext.endOffset === -1) ?
            aContext.bounds : Utils.getTextBounds(aContext.accessibleForBounds,
                                                  aContext.startOffset,
                                                  aContext.endOffset);

      return {
        type: this.type,
        details: {
          eventType: 'vc-change',
          bounds: bounds,
          padding: this.BORDER_PADDING
        }
      };
    } catch (e) {
      Logger.logException(e, 'Failed to get bounds');
      return null;
    }
  };

VisualPresenter.prototype.tabSelected =
  function VisualPresenter_tabSelected(aDocContext, aVCContext) {
    return this.pivotChanged(aVCContext, Ci.nsIAccessiblePivot.REASON_NONE);
  };

VisualPresenter.prototype.tabStateChanged =
  function VisualPresenter_tabStateChanged(aDocObj, aPageState) {
    if (aPageState == 'newdoc') {
      return {type: this.type, details: {eventType: 'tabstate-change'}};
    }

    return null;
  };

/**
 * Android presenter. Fires Android a11y events.
 */
function AndroidPresenter() {}

AndroidPresenter.prototype = Object.create(Presenter.prototype);

AndroidPresenter.prototype.type = 'Android';

// Android AccessibilityEvent type constants.
AndroidPresenter.prototype.ANDROID_VIEW_CLICKED = 0x01;
AndroidPresenter.prototype.ANDROID_VIEW_LONG_CLICKED = 0x02;
AndroidPresenter.prototype.ANDROID_VIEW_SELECTED = 0x04;
AndroidPresenter.prototype.ANDROID_VIEW_FOCUSED = 0x08;
AndroidPresenter.prototype.ANDROID_VIEW_TEXT_CHANGED = 0x10;
AndroidPresenter.prototype.ANDROID_WINDOW_STATE_CHANGED = 0x20;
AndroidPresenter.prototype.ANDROID_VIEW_HOVER_ENTER = 0x80;
AndroidPresenter.prototype.ANDROID_VIEW_HOVER_EXIT = 0x100;
AndroidPresenter.prototype.ANDROID_VIEW_SCROLLED = 0x1000;
AndroidPresenter.prototype.ANDROID_VIEW_TEXT_SELECTION_CHANGED = 0x2000;
AndroidPresenter.prototype.ANDROID_ANNOUNCEMENT = 0x4000;
AndroidPresenter.prototype.ANDROID_VIEW_ACCESSIBILITY_FOCUSED = 0x8000;
AndroidPresenter.prototype.ANDROID_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY =
  0x20000;

AndroidPresenter.prototype.pivotChanged =
  function AndroidPresenter_pivotChanged(aContext, aReason) {
    if (!aContext.accessible) {
      return null;
    }

    let androidEvents = [];

    let isExploreByTouch = (aReason == Ci.nsIAccessiblePivot.REASON_POINT &&
                            Utils.AndroidSdkVersion >= 14);
    let focusEventType = (Utils.AndroidSdkVersion >= 16) ?
      this.ANDROID_VIEW_ACCESSIBILITY_FOCUSED :
      this.ANDROID_VIEW_FOCUSED;

    if (isExploreByTouch) {
      // This isn't really used by TalkBack so this is a half-hearted attempt
      // for now.
      androidEvents.push({eventType: this.ANDROID_VIEW_HOVER_EXIT, text: []});
    }

    let brailleOutput = {};
    if (Utils.AndroidSdkVersion >= 16) {
      if (!this._braillePresenter) {
        this._braillePresenter = new BraillePresenter();
      }
      brailleOutput = this._braillePresenter.pivotChanged(aContext, aReason).
                         details;
    }

    if (aReason === Ci.nsIAccessiblePivot.REASON_TEXT) {
      if (Utils.AndroidSdkVersion >= 16) {
        let adjustedText = aContext.textAndAdjustedOffsets;

        androidEvents.push({
          eventType: this.ANDROID_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY,
          text: [adjustedText.text],
          fromIndex: adjustedText.startOffset,
          toIndex: adjustedText.endOffset
        });
      }
    } else {
      let state = Utils.getState(aContext.accessible);
      androidEvents.push({eventType: (isExploreByTouch) ?
                           this.ANDROID_VIEW_HOVER_ENTER : focusEventType,
                         text: Utils.localize(UtteranceGenerator.genForContext(
                           aContext)),
                         bounds: aContext.bounds,
                         clickable: aContext.accessible.actionCount > 0,
                         checkable: state.contains(States.CHECKABLE),
                         checked: state.contains(States.CHECKED),
                         brailleOutput: brailleOutput});
    }


    return {
      type: this.type,
      details: androidEvents
    };
  };

AndroidPresenter.prototype.actionInvoked =
  function AndroidPresenter_actionInvoked(aObject, aActionName) {
    let state = Utils.getState(aObject);

    // Checkable objects use TalkBack's text derived from the event state,
    // so we don't populate the text here.
    let text = '';
    if (!state.contains(States.CHECKABLE)) {
      text = Utils.localize(UtteranceGenerator.genForAction(aObject,
        aActionName));
    }

    return {
      type: this.type,
      details: [{
        eventType: this.ANDROID_VIEW_CLICKED,
        text: text,
        checked: state.contains(States.CHECKED)
      }]
    };
  };

AndroidPresenter.prototype.tabSelected =
  function AndroidPresenter_tabSelected(aDocContext, aVCContext) {
    // Send a pivot change message with the full context utterance for this doc.
    return this.pivotChanged(aVCContext, Ci.nsIAccessiblePivot.REASON_NONE);
  };

AndroidPresenter.prototype.tabStateChanged =
  function AndroidPresenter_tabStateChanged(aDocObj, aPageState) {
    return this.announce(
      UtteranceGenerator.genForTabStateChange(aDocObj, aPageState));
  };

AndroidPresenter.prototype.textChanged = function AndroidPresenter_textChanged(
  aAccessible, aIsInserted, aStart, aLength, aText, aModifiedText) {
    let eventDetails = {
      eventType: this.ANDROID_VIEW_TEXT_CHANGED,
      text: [aText],
      fromIndex: aStart,
      removedCount: 0,
      addedCount: 0
    };

    if (aIsInserted) {
      eventDetails.addedCount = aLength;
      eventDetails.beforeText =
        aText.substring(0, aStart) + aText.substring(aStart + aLength);
    } else {
      eventDetails.removedCount = aLength;
      eventDetails.beforeText =
        aText.substring(0, aStart) + aModifiedText + aText.substring(aStart);
    }

    return {type: this.type, details: [eventDetails]};
  };

AndroidPresenter.prototype.textSelectionChanged =
  function AndroidPresenter_textSelectionChanged(aText, aStart, aEnd, aOldStart,
                                                 aOldEnd, aIsFromUserInput) {
    let androidEvents = [];

    if (Utils.AndroidSdkVersion >= 14 && !aIsFromUserInput) {
      if (!this._braillePresenter) {
        this._braillePresenter = new BraillePresenter();
      }
      let brailleOutput = this._braillePresenter.textSelectionChanged(
        aText, aStart, aEnd, aOldStart, aOldEnd, aIsFromUserInput).details;

      androidEvents.push({
        eventType: this.ANDROID_VIEW_TEXT_SELECTION_CHANGED,
        text: [aText],
        fromIndex: aStart,
        toIndex: aEnd,
        itemCount: aText.length,
        brailleOutput: brailleOutput
      });
    }

    if (Utils.AndroidSdkVersion >= 16 && aIsFromUserInput) {
      let [from, to] = aOldStart < aStart ?
        [aOldStart, aStart] : [aStart, aOldStart];
      androidEvents.push({
        eventType: this.ANDROID_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY,
        text: [aText],
        fromIndex: from,
        toIndex: to
      });
    }

    return {
      type: this.type,
      details: androidEvents
    };
  };

AndroidPresenter.prototype.viewportChanged =
  function AndroidPresenter_viewportChanged(aWindow) {
    if (Utils.AndroidSdkVersion < 14) {
      return null;
    }

    return {
      type: this.type,
      details: [{
        eventType: this.ANDROID_VIEW_SCROLLED,
        text: [],
        scrollX: aWindow.scrollX,
        scrollY: aWindow.scrollY,
        maxScrollX: aWindow.scrollMaxX,
        maxScrollY: aWindow.scrollMaxY
      }]
    };
  };

AndroidPresenter.prototype.editingModeChanged =
  function AndroidPresenter_editingModeChanged(aIsEditing) {
    return this.announce(UtteranceGenerator.genForEditingMode(aIsEditing));
  };

AndroidPresenter.prototype.announce =
  function AndroidPresenter_announce(aAnnouncement) {
    let localizedAnnouncement = Utils.localize(aAnnouncement).join(' ');
    return {
      type: this.type,
      details: [{
        eventType: (Utils.AndroidSdkVersion >= 16) ?
          this.ANDROID_ANNOUNCEMENT : this.ANDROID_VIEW_TEXT_CHANGED,
        text: [localizedAnnouncement],
        addedCount: localizedAnnouncement.length,
        removedCount: 0,
        fromIndex: 0
      }]
    };
  };

AndroidPresenter.prototype.liveRegion =
  function AndroidPresenter_liveRegion(aContext, aIsPolite,
    aIsHide, aModifiedText) {
    return this.announce(
      UtteranceGenerator.genForLiveRegion(aContext, aIsHide, aModifiedText));
  };

AndroidPresenter.prototype.noMove =
  function AndroidPresenter_noMove(aMoveMethod) {
    return {
      type: this.type,
      details: [
      { eventType: this.ANDROID_VIEW_ACCESSIBILITY_FOCUSED,
        exitView: aMoveMethod,
        text: ['']
      }]
    };
  };

/**
 * A B2G presenter for Gaia.
 */
function B2GPresenter() {}

B2GPresenter.prototype = Object.create(Presenter.prototype);

B2GPresenter.prototype.type = 'B2G';

B2GPresenter.prototype.keyboardEchoSetting =
  new PrefCache('accessibility.accessfu.keyboard_echo');
B2GPresenter.prototype.NO_ECHO = 0;
B2GPresenter.prototype.CHARACTER_ECHO = 1;
B2GPresenter.prototype.WORD_ECHO = 2;
B2GPresenter.prototype.CHARACTER_AND_WORD_ECHO = 3;

/**
 * A pattern used for haptic feedback.
 * @type {Array}
 */
B2GPresenter.prototype.PIVOT_CHANGE_HAPTIC_PATTERN = [40];

/**
 * Pivot move reasons.
 * @type {Array}
 */
B2GPresenter.prototype.pivotChangedReasons = ['none', 'next', 'prev', 'first',
                                              'last', 'text', 'point'];

B2GPresenter.prototype.pivotChanged =
  function B2GPresenter_pivotChanged(aContext, aReason, aIsUserInput) {
    if (!aContext.accessible) {
      return null;
    }

    return {
      type: this.type,
      details: {
        eventType: 'vc-change',
        data: UtteranceGenerator.genForContext(aContext),
        options: {
          pattern: this.PIVOT_CHANGE_HAPTIC_PATTERN,
          isKey: Utils.isActivatableOnFingerUp(aContext.accessible),
          reason: this.pivotChangedReasons[aReason],
          isUserInput: aIsUserInput,
          hints: aContext.interactionHints
        }
      }
    };
  };

B2GPresenter.prototype.nameChanged =
  function B2GPresenter_nameChanged(aAccessible) {
    return {
      type: this.type,
      details: {
        eventType: 'name-change',
        data: aAccessible.name
      }
    };
  };

B2GPresenter.prototype.valueChanged =
  function B2GPresenter_valueChanged(aAccessible) {

    // the editable value changes are handled in the text changed presenter
    if (Utils.getState(aAccessible).contains(States.EDITABLE)) {
      return null;
    }

    return {
      type: this.type,
      details: {
        eventType: 'value-change',
        data: aAccessible.value
      }
    };
  };

B2GPresenter.prototype.textChanged = function B2GPresenter_textChanged(
  aAccessible, aIsInserted, aStart, aLength, aText, aModifiedText) {
    let echoSetting = this.keyboardEchoSetting.value;
    let text = '';

    if (echoSetting == this.CHARACTER_ECHO ||
        echoSetting == this.CHARACTER_AND_WORD_ECHO) {
      text = aModifiedText;
    }

    // add word if word boundary is added
    if ((echoSetting == this.WORD_ECHO ||
        echoSetting == this.CHARACTER_AND_WORD_ECHO) &&
        aIsInserted && aLength === 1) {
      let accText = aAccessible.QueryInterface(Ci.nsIAccessibleText);
      let startBefore = {}, endBefore = {};
      let startAfter = {}, endAfter = {};
      accText.getTextBeforeOffset(aStart,
        Ci.nsIAccessibleText.BOUNDARY_WORD_END, startBefore, endBefore);
      let maybeWord = accText.getTextBeforeOffset(aStart + 1,
        Ci.nsIAccessibleText.BOUNDARY_WORD_END, startAfter, endAfter);
      if (endBefore.value !== endAfter.value) {
        text += maybeWord;
      }
    }

    return {
      type: this.type,
      details: {
        eventType: 'text-change',
        data: text
      }
    };

  };

B2GPresenter.prototype.actionInvoked =
  function B2GPresenter_actionInvoked(aObject, aActionName) {
    return {
      type: this.type,
      details: {
        eventType: 'action',
        data: UtteranceGenerator.genForAction(aObject, aActionName)
      }
    };
  };

B2GPresenter.prototype.liveRegion = function B2GPresenter_liveRegion(aContext,
  aIsPolite, aIsHide, aModifiedText) {
    return {
      type: this.type,
      details: {
        eventType: 'liveregion-change',
        data: UtteranceGenerator.genForLiveRegion(aContext, aIsHide,
          aModifiedText),
        options: {enqueue: aIsPolite}
      }
    };
  };

B2GPresenter.prototype.announce =
  function B2GPresenter_announce(aAnnouncement) {
    return {
      type: this.type,
      details: {
        eventType: 'announcement',
        data: aAnnouncement
      }
    };
  };

B2GPresenter.prototype.noMove =
  function B2GPresenter_noMove(aMoveMethod) {
    return {
      type: this.type,
      details: {
        eventType: 'no-move',
        data: aMoveMethod
      }
    };
  };

/**
 * A braille presenter
 */
function BraillePresenter() {}

BraillePresenter.prototype = Object.create(Presenter.prototype);

BraillePresenter.prototype.type = 'Braille';

BraillePresenter.prototype.pivotChanged =
  function BraillePresenter_pivotChanged(aContext) {
    if (!aContext.accessible) {
      return null;
    }

    return {
      type: this.type,
      details: {
        output: Utils.localize(BrailleGenerator.genForContext(aContext)).join(
          ' '),
        selectionStart: 0,
        selectionEnd: 0
      }
    };
  };

BraillePresenter.prototype.textSelectionChanged =
  function BraillePresenter_textSelectionChanged(aText, aStart, aEnd) {
    return {
      type: this.type,
      details: {
        selectionStart: aStart,
        selectionEnd: aEnd
      }
    };
  };

this.Presentation = { // jshint ignore:line
  get presenters() {
    delete this.presenters;
    let presenterMap = {
      'mobile/android': [VisualPresenter, AndroidPresenter],
      'b2g': [VisualPresenter, B2GPresenter],
      'browser': [VisualPresenter, B2GPresenter, AndroidPresenter]
    };
    this.presenters = [new P() for (P of presenterMap[Utils.MozBuildApp])]; // jshint ignore:line
    return this.presenters;
  },

  pivotChanged: function Presentation_pivotChanged(
    aPosition, aOldPosition, aReason, aStartOffset, aEndOffset, aIsUserInput) {
    let context = new PivotContext(
      aPosition, aOldPosition, aStartOffset, aEndOffset);
    return [p.pivotChanged(context, aReason, aIsUserInput)
      for each (p in this.presenters)]; // jshint ignore:line
  },

  actionInvoked: function Presentation_actionInvoked(aObject, aActionName) {
    return [p.actionInvoked(aObject, aActionName) // jshint ignore:line
      for each (p in this.presenters)]; // jshint ignore:line
  },

  textChanged: function Presentation_textChanged(aAccessible, aIsInserted,
                                    aStartOffset, aLength, aText,
                                    aModifiedText) {
    return [p.textChanged(aAccessible, aIsInserted, aStartOffset, aLength, // jshint ignore:line
      aText, aModifiedText) for each (p in this.presenters)]; // jshint ignore:line
  },

  textSelectionChanged: function textSelectionChanged(aText, aStart, aEnd,
                                                      aOldStart, aOldEnd,
                                                      aIsFromUserInput) {
    return [p.textSelectionChanged(aText, aStart, aEnd, aOldStart, aOldEnd, // jshint ignore:line
      aIsFromUserInput) for each (p in this.presenters)]; // jshint ignore:line
  },

  nameChanged: function nameChanged(aAccessible) {
    return [ p.nameChanged(aAccessible) for (p of this.presenters) ]; // jshint ignore:line
  },

  valueChanged: function valueChanged(aAccessible) {
    return [ p.valueChanged(aAccessible) for (p of this.presenters) ]; // jshint ignore:line
  },

  tabStateChanged: function Presentation_tabStateChanged(aDocObj, aPageState) {
    return [p.tabStateChanged(aDocObj, aPageState) // jshint ignore:line
      for each (p in this.presenters)]; // jshint ignore:line
  },

  viewportChanged: function Presentation_viewportChanged(aWindow) {
    return [p.viewportChanged(aWindow) for each (p in this.presenters)]; // jshint ignore:line
  },

  editingModeChanged: function Presentation_editingModeChanged(aIsEditing) {
    return [p.editingModeChanged(aIsEditing) for each (p in this.presenters)]; // jshint ignore:line
  },

  announce: function Presentation_announce(aAnnouncement) {
    // XXX: Typically each presenter uses the UtteranceGenerator,
    // but there really isn't a point here.
    return [p.announce(UtteranceGenerator.genForAnnouncement(aAnnouncement)) // jshint ignore:line
      for each (p in this.presenters)]; // jshint ignore:line
  },

  noMove: function Presentation_noMove(aMoveMethod) {
    return [p.noMove(aMoveMethod) for each (p in this.presenters)]; // jshint ignore:line
  },

  liveRegion: function Presentation_liveRegion(aAccessible, aIsPolite, aIsHide,
    aModifiedText) {
    let context;
    if (!aModifiedText) {
      context = new PivotContext(aAccessible, null, -1, -1, true,
        aIsHide ? true : false);
    }
    return [p.liveRegion(context, aIsPolite, aIsHide, aModifiedText) // jshint ignore:line
      for (p of this.presenters)]; // jshint ignore:line
  }
};
