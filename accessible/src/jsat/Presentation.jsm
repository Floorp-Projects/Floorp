/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import('resource://gre/modules/accessibility/Utils.jsm');
Cu.import('resource://gre/modules/accessibility/UtteranceGenerator.jsm');
Cu.import('resource://gre/modules/Geometry.jsm');

this.EXPORTED_SYMBOLS = ['Presentation'];

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
   * Attach function for presenter.
   * @param {ChromeWindow} aWindow Chrome window the presenter could use.
   */
  attach: function attach(aWindow) {},

  /**
   * Detach function.
   */
  detach: function detach() {},

  /**
   * The virtual cursor's position changed.
   * @param {PresenterContext} aContext the context object for the new pivot
   *   position.
   * @param {int} aReason the reason for the pivot change.
   *   See nsIAccessiblePivot.
   */
  pivotChanged: function pivotChanged(aContext, aReason) {},

  /**
   * An object's action has been invoked.
   * @param {nsIAccessible} aObject the object that has been invoked.
   * @param {string} aActionName the name of the action.
   */
  actionInvoked: function actionInvoked(aObject, aActionName) {},

  /**
   * Text has changed, either by the user or by the system. TODO.
   */
  textChanged: function textChanged(aIsInserted, aStartOffset,
                                    aLength, aText,
                                    aModifiedText) {},

  /**
   * Text selection has changed. TODO.
   */
  textSelectionChanged: function textSelectionChanged() {},

  /**
   * Selection has changed. TODO.
   * @param {nsIAccessible} aObject the object that has been selected.
   */
  selectionChanged: function selectionChanged(aObject) {},

  /**
   * The tab, or the tab's document state has changed.
   * @param {nsIAccessible} aDocObj the tab document accessible that has had its
   *    state changed, or null if the tab has no associated document yet.
   * @param {string} aPageState the state name for the tab, valid states are:
   *    'newtab', 'loading', 'newdoc', 'loaded', 'stopped', and 'reload'.
   */
  tabStateChanged: function tabStateChanged(aDocObj, aPageState) {},

  /**
   * The current tab has changed.
   * @param {PresenterContext} aDocContext context object for tab's
   *   document.
   * @param {PresenterContext} aVCContext context object for tab's current
   *   virtual cursor position.
   */
  tabSelected: function tabSelected(aDocContext, aVCContext) {},

  /**
   * The viewport has changed, either a scroll, pan, zoom, or
   *    landscape/portrait toggle.
   * @param {Window} aWindow window of viewport that changed.
   */
  viewportChanged: function viewportChanged(aWindow) {},

  /**
   * We have entered or left text editing mode.
   */
  editingModeChanged: function editingModeChanged(aIsEditing) {},

  /**
   * Announce something. Typically an app state change.
   */
  announce: function announce(aAnnouncement) {}
};

/**
 * Visual presenter. Draws a box around the virtual cursor's position.
 */

this.VisualPresenter = function VisualPresenter() {};

VisualPresenter.prototype = {
  __proto__: Presenter.prototype,

  type: 'Visual',

  /**
   * The padding in pixels between the object and the highlight border.
   */
  BORDER_PADDING: 2,

  viewportChanged: function VisualPresenter_viewportChanged(aWindow) {
    if (this._currentAccessible) {
      let context = new PresenterContext(this._currentAccessible);
      return {
        type: this.type,
        details: {
          method: 'showBounds',
          bounds: context.bounds,
          padding: this.BORDER_PADDING
        }
      };
    }

    return null;
  },

  pivotChanged: function VisualPresenter_pivotChanged(aContext, aReason) {
    this._currentAccessible = aContext.accessible;

    if (!aContext.accessible)
      return {type: this.type, details: {method: 'hideBounds'}};

    try {
      aContext.accessible.scrollTo(
        Ci.nsIAccessibleScrollType.SCROLL_TYPE_ANYWHERE);
      return {
        type: this.type,
        details: {
          method: 'showBounds',
          bounds: aContext.bounds,
          padding: this.BORDER_PADDING
        }
      };
    } catch (e) {
      Logger.error('Failed to get bounds: ' + e);
      return null;
    }
  },

  tabSelected: function VisualPresenter_tabSelected(aDocContext, aVCContext) {
    return this.pivotChanged(aVCContext, Ci.nsIAccessiblePivot.REASON_NONE);
  },

  tabStateChanged: function VisualPresenter_tabStateChanged(aDocObj,
                                                            aPageState) {
    if (aPageState == 'newdoc')
      return {type: this.type, details: {method: 'hideBounds'}};

    return null;
  },

  announce: function VisualPresenter_announce(aAnnouncement) {
    return {
      type: this.type,
      details: {
        method: 'showAnnouncement',
        text: aAnnouncement,
        duration: 1000
      }
    };
  }
};

/**
 * Android presenter. Fires Android a11y events.
 */

this.AndroidPresenter = function AndroidPresenter() {};

AndroidPresenter.prototype = {
  __proto__: Presenter.prototype,

  type: 'Android',

  // Android AccessibilityEvent type constants.
  ANDROID_VIEW_CLICKED: 0x01,
  ANDROID_VIEW_LONG_CLICKED: 0x02,
  ANDROID_VIEW_SELECTED: 0x04,
  ANDROID_VIEW_FOCUSED: 0x08,
  ANDROID_VIEW_TEXT_CHANGED: 0x10,
  ANDROID_WINDOW_STATE_CHANGED: 0x20,
  ANDROID_VIEW_HOVER_ENTER: 0x80,
  ANDROID_VIEW_HOVER_EXIT: 0x100,
  ANDROID_VIEW_SCROLLED: 0x1000,
  ANDROID_ANNOUNCEMENT: 0x4000,
  ANDROID_VIEW_ACCESSIBILITY_FOCUSED: 0x8000,

  pivotChanged: function AndroidPresenter_pivotChanged(aContext, aReason) {
    if (!aContext.accessible)
      return null;

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

    let output = [];

    aContext.newAncestry.forEach(
      function(acc) {
        output.push.apply(output, UtteranceGenerator.genForObject(acc));
      }
    );

    output.push.apply(output,
                      UtteranceGenerator.genForObject(aContext.accessible));

    aContext.subtreePreorder.forEach(
      function(acc) {
        output.push.apply(output, UtteranceGenerator.genForObject(acc));
      }
    );

    androidEvents.push({eventType: (isExploreByTouch) ?
                          this.ANDROID_VIEW_HOVER_ENTER : focusEventType,
                        text: output,
                        bounds: aContext.bounds});
    return {
      type: this.type,
      details: androidEvents
    };
  },

  actionInvoked: function AndroidPresenter_actionInvoked(aObject, aActionName) {
    return {
      type: this.type,
      details: [{
        eventType: this.ANDROID_VIEW_CLICKED,
        text: UtteranceGenerator.genForAction(aObject, aActionName)
      }]
    };
  },

  tabSelected: function AndroidPresenter_tabSelected(aDocContext, aVCContext) {
    // Send a pivot change message with the full context utterance for this doc.
    return this.pivotChanged(aVCContext, Ci.nsIAccessiblePivot.REASON_NONE);
  },

  tabStateChanged: function AndroidPresenter_tabStateChanged(aDocObj,
                                                             aPageState) {
    return this.announce(
      UtteranceGenerator.genForTabStateChange(aDocObj, aPageState).join(' '));
  },

  textChanged: function AndroidPresenter_textChanged(aIsInserted, aStart,
                                                     aLength, aText,
                                                     aModifiedText) {
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
  },

  viewportChanged: function AndroidPresenter_viewportChanged(aWindow) {
    if (Utils.AndroidSdkVersion < 14)
      return null;

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
  },

  editingModeChanged: function AndroidPresenter_editingModeChanged(aIsEditing) {
    return this.announce(
      UtteranceGenerator.genForEditingMode(aIsEditing).join(' '));
  },

  announce: function AndroidPresenter_announce(aAnnouncement) {
    return {
      type: this.type,
      details: [{
        eventType: (Utils.AndroidSdkVersion >= 16) ?
          this.ANDROID_ANNOUNCEMENT : this.ANDROID_VIEW_TEXT_CHANGED,
        text: [aAnnouncement],
        addedCount: aAnnouncement.length,
        removedCount: 0,
        fromIndex: 0
      }]
    };
  }
};

/**
 * A speech presenter for direct TTS output
 */

this.SpeechPresenter = function SpeechPresenter() {};

SpeechPresenter.prototype = {
  __proto__: Presenter.prototype,

  type: 'Speech',

  pivotChanged: function SpeechPresenter_pivotChanged(aContext, aReason) {
    if (!aContext.accessible)
      return null;

    let output = [];

    aContext.newAncestry.forEach(
      function(acc) {
        output.push.apply(output, UtteranceGenerator.genForObject(acc));
      }
    );

    output.push.apply(output,
                      UtteranceGenerator.genForObject(aContext.accessible));

    aContext.subtreePreorder.forEach(
      function(acc) {
        output.push.apply(output, UtteranceGenerator.genForObject(acc));
      }
    );

    return {
      type: this.type,
      details: {
        actions: [
          {method: 'playEarcon', data: 'tick', options: {}},
          {method: 'speak', data: output.join(' '), options: {enqueue: true}}
        ]
      }
    };
  }
};

/**
 * A haptic presenter
 */

this.HapticPresenter = function HapticPresenter() {};

HapticPresenter.prototype = {
  __proto__: Presenter.prototype,

  type: 'Haptic',

  PIVOT_CHANGE_PATTHERN: [20],

  pivotChanged: function HapticPresenter_pivotChanged(aContext, aReason) {
    return { type: this.type, details: { pattern: this.PIVOT_CHANGE_PATTHERN } };
  }
};

/**
 * PresenterContext: An object that generates and caches context information
 * for a given accessible and its relationship with another accessible.
 */
this.PresenterContext = function PresenterContext(aAccessible, aOldAccessible) {
  this._accessible = aAccessible;
  this._oldAccessible =
    this._isDefunct(aOldAccessible) ? null : aOldAccessible;
}

PresenterContext.prototype = {
  get accessible() {
    return this._accessible;
  },

  get oldAccessible() {
    return this._oldAccessible;
  },

  /*
   * This is a list of the accessible's ancestry up to the common ancestor
   * of the accessible and the old accessible. It is useful for giving the
   * user context as to where they are in the heirarchy.
   */
  get newAncestry() {
    if (!this._newAncestry) {
      let newLineage = [];
      let oldLineage = [];

      let parent = this._accessible;
      while (parent && (parent = parent.parent))
        newLineage.push(parent);

      parent = this._oldAccessible;
      while (parent && (parent = parent.parent))
        oldLineage.push(parent);

      this._newAncestry = [];

      while (true) {
        let newAncestor = newLineage.pop();
        let oldAncestor = oldLineage.pop();

        if (newAncestor == undefined)
          break;

        if (newAncestor != oldAncestor)
          this._newAncestry.push(newAncestor);
      }

    }

    return this._newAncestry;
  },

  /*
   * This is a flattened list of the accessible's subtree in preorder.
   * It only includes the accessible's visible chidren.
   */
  get subtreePreorder() {
    function traversePreorder(aAccessible) {
      let list = [];
      let child = aAccessible.firstChild;
      while (child) {
        let state = {};
        child.getState(state, {});

        if (!(state.value & Ci.nsIAccessibleStates.STATE_INVISIBLE)) {
          list.push(child);
          list.push.apply(list, traversePreorder(child));
        }

        child = child.nextSibling;
      }
      return list;
    }

    if (!this._subtreePreOrder)
      this._subtreePreOrder = traversePreorder(this._accessible);

    return this._subtreePreOrder;
  },

  get bounds() {
    if (!this._bounds) {
      let objX = {}, objY = {}, objW = {}, objH = {};

      this._accessible.getBounds(objX, objY, objW, objH);

      // XXX: OOP content provides a screen offset of 0, while in-process provides a real
      // offset. Removing the offset and using content-relative coords normalizes this.
      let docX = {}, docY = {};
      let docRoot = this._accessible.rootDocument.
        QueryInterface(Ci.nsIAccessible);
      docRoot.getBounds(docX, docY, {}, {});

      this._bounds = new Rect(objX.value, objY.value, objW.value, objH.value).
        translate(-docX.value, -docY.value);
    }

    return this._bounds.clone();
  },

  _isDefunct: function _isDefunct(aAccessible) {
    try {
      let extstate = {};
      aAccessible.getState({}, extstate);
      return !!(aAccessible.value & Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT);
    } catch (x) {
      return true;
    }
  }
};

this.Presentation = {
  get presenters() {
    delete this.presenters;
    this.presenters = [new VisualPresenter()];

    if (Utils.MozBuildApp == 'b2g') {
      this.presenters.push(new SpeechPresenter());
      this.presenters.push(new HapticPresenter());
    } else if (Utils.MozBuildApp == 'mobile/android') {
      this.presenters.push(new AndroidPresenter());
    }

    return this.presenters;
  },

  pivotChanged: function Presentation_pivotChanged(aPosition,
                                                   aOldPosition,
                                                   aReason) {
    let context = new PresenterContext(aPosition, aOldPosition);
    return [p.pivotChanged(context, aReason)
              for each (p in this.presenters)];
  },

  actionInvoked: function Presentation_actionInvoked(aObject, aActionName) {
    return [p.actionInvoked(aObject, aActionName)
              for each (p in this.presenters)];
  },

  textChanged: function Presentation_textChanged(aIsInserted, aStartOffset,
                                    aLength, aText,
                                    aModifiedText) {
    return [p.textChanged(aIsInserted, aStartOffset, aLength,
                          aText, aModifiedText)
              for each (p in this.presenters)];
  },

  tabStateChanged: function Presentation_tabStateChanged(aDocObj, aPageState) {
    return [p.tabStateChanged(aDocObj, aPageState)
              for each (p in this.presenters)];
  },

  viewportChanged: function Presentation_viewportChanged(aWindow) {
    return [p.viewportChanged(aWindow)
              for each (p in this.presenters)];
  },

  editingModeChanged: function Presentation_editingModeChanged(aIsEditing) {
    return [p.editingModeChanged(aIsEditing)
              for each (p in this.presenters)];
  },

  announce: function Presentation_announce(aAnnouncement) {
    // XXX: Typically each presenter uses the UtteranceGenerator,
    // but there really isn't a point here.
    return [p.announce(UtteranceGenerator.genForAnnouncement(aAnnouncement)[0])
              for each (p in this.presenters)];
  }
};
