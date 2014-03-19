/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Utils',
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Logger',
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'PivotContext',
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'UtteranceGenerator',
  'resource://gre/modules/accessibility/OutputGenerator.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'BrailleGenerator',
  'resource://gre/modules/accessibility/OutputGenerator.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Roles',
  'resource://gre/modules/accessibility/Constants.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'States',
  'resource://gre/modules/accessibility/Constants.jsm');

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
   * The virtual cursor's position changed.
   * @param {PivotContext} aContext the context object for the new pivot
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
  textSelectionChanged: function textSelectionChanged(aText, aStart, aEnd, aOldStart, aOldEnd, aIsFromUser) {},

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
   * @param {PivotContext} aDocContext context object for tab's
   *   document.
   * @param {PivotContext} aVCContext context object for tab's current
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
  announce: function announce(aAnnouncement) {},



  /**
   * Announce a live region.
   * @param  {PivotContext} aContext context object for an accessible.
   * @param  {boolean} aIsPolite A politeness level for a live region.
   * @param  {boolean} aIsHide An indicator of hide/remove event.
   * @param  {string} aModifiedText Optional modified text.
   */
  liveRegion: function liveRegionShown(aContext, aIsPolite, aIsHide,
    aModifiedText) {}
};

/**
 * Visual presenter. Draws a box around the virtual cursor's position.
 */

this.VisualPresenter = function VisualPresenter() {
  this._displayedAccessibles = new WeakMap();
};

VisualPresenter.prototype = {
  __proto__: Presenter.prototype,

  type: 'Visual',

  /**
   * The padding in pixels between the object and the highlight border.
   */
  BORDER_PADDING: 2,

  viewportChanged: function VisualPresenter_viewportChanged(aWindow) {
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
          method: 'showBounds',
          bounds: bounds,
          padding: this.BORDER_PADDING
        }
      };
    }

    return null;
  },

  pivotChanged: function VisualPresenter_pivotChanged(aContext, aReason) {
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
          method: 'showBounds',
          bounds: bounds,
          padding: this.BORDER_PADDING
        }
      };
    } catch (e) {
      Logger.logException(e, 'Failed to get bounds');
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
  ANDROID_VIEW_TEXT_SELECTION_CHANGED: 0x2000,
  ANDROID_ANNOUNCEMENT: 0x4000,
  ANDROID_VIEW_ACCESSIBILITY_FOCUSED: 0x8000,
  ANDROID_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY: 0x20000,

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
                         text: UtteranceGenerator.genForContext(aContext).output,
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
  },

  actionInvoked: function AndroidPresenter_actionInvoked(aObject, aActionName) {
    let state = Utils.getState(aObject);

    // Checkable objects will have a state changed event we will use instead.
    if (state.contains(States.CHECKABLE))
      return null;

    return {
      type: this.type,
      details: [{
        eventType: this.ANDROID_VIEW_CLICKED,
        text: UtteranceGenerator.genForAction(aObject, aActionName),
        checked: state.contains(States.CHECKED)
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

  textSelectionChanged: function AndroidPresenter_textSelectionChanged(aText, aStart,
                                                                       aEnd, aOldStart,
                                                                       aOldEnd, aIsFromUser) {
    let androidEvents = [];

    if (Utils.AndroidSdkVersion >= 14 && !aIsFromUser) {
      if (!this._braillePresenter) {
        this._braillePresenter = new BraillePresenter();
      }
      let brailleOutput = this._braillePresenter.textSelectionChanged(aText, aStart, aEnd,
                                                                      aOldStart, aOldEnd,
                                                                      aIsFromUser).details;

      androidEvents.push({
        eventType: this.ANDROID_VIEW_TEXT_SELECTION_CHANGED,
        text: [aText],
        fromIndex: aStart,
        toIndex: aEnd,
        itemCount: aText.length,
        brailleOutput: brailleOutput
      });
    }

    if (Utils.AndroidSdkVersion >= 16 && aIsFromUser) {
      let [from, to] = aOldStart < aStart ? [aOldStart, aStart] : [aStart, aOldStart];
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
  },

  liveRegion: function AndroidPresenter_liveRegion(aContext, aIsPolite,
    aIsHide, aModifiedText) {
    return this.announce(
      UtteranceGenerator.genForLiveRegion(aContext, aIsHide,
        aModifiedText).join(' '));
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

    return {
      type: this.type,
      details: {
        actions: [
          {method: 'playEarcon',
           data: aContext.accessible.role === Roles.KEY ?
             'virtual_cursor_key' : 'virtual_cursor_move',
           options: {}},
          {method: 'speak',
            data: UtteranceGenerator.genForContext(aContext).output.join(' '),
            options: {enqueue: true}}
        ]
      }
    };
  },

  actionInvoked: function SpeechPresenter_actionInvoked(aObject, aActionName) {
    let actions = [];
    if (aActionName === 'click') {
      actions.push({method: 'playEarcon',
                    data: 'clicked',
                    options: {}});
    } else {
      actions.push({method: 'speak',
                    data: UtteranceGenerator.genForAction(aObject, aActionName).join(' '),
                    options: {enqueue: false}});
    }
    return { type: this.type, details: { actions: actions } };
  },

  liveRegion: function SpeechPresenter_liveRegion(aContext, aIsPolite, aIsHide,
    aModifiedText) {
    return {
      type: this.type,
      details: {
        actions: [{
          method: 'speak',
          data: UtteranceGenerator.genForLiveRegion(aContext, aIsHide,
            aModifiedText).join(' '),
          options: {enqueue: aIsPolite}
        }]
      }
    };
  },

  announce: function SpeechPresenter_announce(aAnnouncement) {
    return {
      type: this.type,
      details: {
        actions: [{
          method: 'speak', data: aAnnouncement, options: { enqueue: false }
        }]
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

  PIVOT_CHANGE_PATTERN: [40],

  pivotChanged: function HapticPresenter_pivotChanged(aContext, aReason) {
    return { type: this.type, details: { pattern: this.PIVOT_CHANGE_PATTERN } };
  }
};

/**
 * A braille presenter
 */

this.BraillePresenter = function BraillePresenter() {};

BraillePresenter.prototype = {
  __proto__: Presenter.prototype,

  type: 'Braille',

  pivotChanged: function BraillePresenter_pivotChanged(aContext, aReason) {
    if (!aContext.accessible) {
      return null;
    }

    let brailleOutput = BrailleGenerator.genForContext(aContext);
    brailleOutput.output = brailleOutput.output.join(' ');
    brailleOutput.selectionStart = 0;
    brailleOutput.selectionEnd = 0;

    return { type: this.type, details: brailleOutput };
  },

  textSelectionChanged: function BraillePresenter_textSelectionChanged(aText, aStart,
                                                                       aEnd, aOldStart,
                                                                       aOldEnd, aIsFromUser) {
    return { type: this.type,
             details: { selectionStart: aStart,
                        selectionEnd: aEnd } };
  },


};

this.Presentation = {
  get presenters() {
    delete this.presenters;
    let presenterMap = {
      'mobile/android': [VisualPresenter, AndroidPresenter],
      'b2g': [VisualPresenter, SpeechPresenter, HapticPresenter],
      'browser': [VisualPresenter, SpeechPresenter, HapticPresenter,
                  AndroidPresenter]
    };
    this.presenters = [new P() for (P of presenterMap[Utils.MozBuildApp])];
    return this.presenters;
  },

  pivotChanged: function Presentation_pivotChanged(aPosition, aOldPosition, aReason,
                                                   aStartOffset, aEndOffset) {
    let context = new PivotContext(aPosition, aOldPosition, aStartOffset, aEndOffset);
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

  textSelectionChanged: function textSelectionChanged(aText, aStart, aEnd,
                                                      aOldStart, aOldEnd,
                                                      aIsFromUser) {
    return [p.textSelectionChanged(aText, aStart, aEnd,
                                   aOldStart, aOldEnd, aIsFromUser)
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
  },

  liveRegion: function Presentation_liveRegion(aAccessible, aIsPolite, aIsHide,
    aModifiedText) {
    let context;
    if (!aModifiedText) {
      context = new PivotContext(aAccessible, null, -1, -1, true,
        aIsHide ? true : false);
    }
    return [p.liveRegion(context, aIsPolite, aIsHide, aModifiedText) for (
      p of this.presenters)];
  }
};
