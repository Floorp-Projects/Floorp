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

var EXPORTED_SYMBOLS = ['VisualPresenter',
                        'AndroidPresenter',
                        'DummyAndroidPresenter',
                        'SpeechPresenter',
                        'PresenterContext'];

/**
 * The interface for all presenter classes. A presenter could be, for example,
 * a speech output module, or a visual cursor indicator.
 */
function Presenter() {}

Presenter.prototype = {
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
   */
  viewportChanged: function viewportChanged() {},

  /**
   * We have entered or left text editing mode.
   */
  editingModeChanged: function editingModeChanged(aIsEditing) {}
};

/**
 * Visual presenter. Draws a box around the virtual cursor's position.
 */

function VisualPresenter() {}

VisualPresenter.prototype = {
  __proto__: Presenter.prototype,

  /**
   * The padding in pixels between the object and the highlight border.
   */
  BORDER_PADDING: 2,

  attach: function VisualPresenter_attach(aWindow) {
    this.chromeWin = aWindow;

    // Add stylesheet
    let stylesheetURL = 'chrome://global/content/accessibility/AccessFu.css';
    this.stylesheet = aWindow.document.createProcessingInstruction(
      'xml-stylesheet', 'href="' + stylesheetURL + '" type="text/css"');
    aWindow.document.insertBefore(this.stylesheet, aWindow.document.firstChild);

    // Add highlight box
    this.highlightBox = this.chromeWin.document.
      createElementNS('http://www.w3.org/1999/xhtml', 'div');
    this.chromeWin.document.documentElement.appendChild(this.highlightBox);
    this.highlightBox.id = 'virtual-cursor-box';

    // Add highlight inset for inner shadow
    let inset = this.chromeWin.document.
      createElementNS('http://www.w3.org/1999/xhtml', 'div');
    inset.id = 'virtual-cursor-inset';

    this.highlightBox.appendChild(inset);
  },

  detach: function VisualPresenter_detach() {
    this.chromeWin.document.removeChild(this.stylesheet);
    this.highlightBox.parentNode.removeChild(this.highlightBox);
    this.highlightBox = this.stylesheet = null;
  },

  viewportChanged: function VisualPresenter_viewportChanged() {
    if (this._currentObject)
      this._highlight(this._currentObject);
  },

  pivotChanged: function VisualPresenter_pivotChanged(aContext, aReason) {
    this._currentObject = aContext.accessible;

    if (!aContext.accessible) {
      this._hide();
      return;
    }

    try {
      aContext.accessible.scrollTo(
        Ci.nsIAccessibleScrollType.SCROLL_TYPE_ANYWHERE);
      this._highlight(aContext.accessible);
    } catch (e) {
      Logger.error('Failed to get bounds: ' + e);
      return;
    }
  },

  tabSelected: function VisualPresenter_tabSelected(aDocContext, aVCContext) {
    this.pivotChanged(aVCContext, Ci.nsIAccessiblePivot.REASON_NONE);
  },

  tabStateChanged: function VisualPresenter_tabStateChanged(aDocObj,
                                                            aPageState) {
    if (aPageState == 'newdoc')
      this._hide();
  },

  // Internals

  _hide: function _hide() {
    this.highlightBox.style.display = 'none';
  },

  _highlight: function _highlight(aObject) {
    let vp = Utils.getViewport(this.chromeWin) || { zoom: 1.0, offsetY: 0 };
    let bounds = this._getBounds(aObject, vp.zoom);

    // First hide it to avoid flickering when changing the style.
    this.highlightBox.style.display = 'none';
    this.highlightBox.style.top = bounds.top + 'px';
    this.highlightBox.style.left = bounds.left + 'px';
    this.highlightBox.style.width = bounds.width + 'px';
    this.highlightBox.style.height = bounds.height + 'px';
    this.highlightBox.style.display = 'block';
  },

  _getBounds: function _getBounds(aObject, aZoom, aStart, aEnd) {
    let objX = {}, objY = {}, objW = {}, objH = {};

    if (aEnd >= 0 && aStart >= 0 && aEnd != aStart) {
      // TODO: Get bounds for text ranges. Leaving this blank until we have
      // proper text navigation in the virtual cursor.
    }

    aObject.getBounds(objX, objY, objW, objH);

    // Can't specify relative coords in nsIAccessible.getBounds, so we do it.
    let docX = {}, docY = {};
    let docRoot = aObject.rootDocument.QueryInterface(Ci.nsIAccessible);
    docRoot.getBounds(docX, docY, {}, {});

    let rv = {
      left: Math.round((objX.value - docX.value - this.BORDER_PADDING) * aZoom),
      top: Math.round((objY.value - docY.value - this.BORDER_PADDING) * aZoom),
      width: Math.round((objW.value + (this.BORDER_PADDING * 2)) * aZoom),
      height: Math.round((objH.value + (this.BORDER_PADDING * 2)) * aZoom)
    };

    return rv;
  }
};

/**
 * Android presenter. Fires Android a11y events.
 */

function AndroidPresenter() {}

AndroidPresenter.prototype = {
  __proto__: Presenter.prototype,

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

  attach: function AndroidPresenter_attach(aWindow) {
    this.chromeWin = aWindow;
  },

  pivotChanged: function AndroidPresenter_pivotChanged(aContext, aReason) {
    if (!aContext.accessible)
      return;

    let isExploreByTouch = (aReason == Ci.nsIAccessiblePivot.REASON_POINT &&
                            Utils.AndroidSdkVersion >= 14);

    if (isExploreByTouch) {
      // This isn't really used by TalkBack so this is a half-hearted attempt
      // for now.
      this.sendMessageToJava({
         gecko: {
           type: 'Accessibility:Event',
           eventType: this.ANDROID_VIEW_HOVER_EXIT,
           text: []
         }
      });
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

    this.sendMessageToJava({
      gecko: {
        type: 'Accessibility:Event',
        eventType: isExploreByTouch ?
          this.ANDROID_VIEW_HOVER_ENTER :
          this.ANDROID_VIEW_FOCUSED,
        text: output
      }
    });
  },

  actionInvoked: function AndroidPresenter_actionInvoked(aObject, aActionName) {
    this.sendMessageToJava({
      gecko: {
        type: 'Accessibility:Event',
        eventType: this.ANDROID_VIEW_CLICKED,
        text: UtteranceGenerator.genForAction(aObject, aActionName)
      }
    });
  },

  tabSelected: function AndroidPresenter_tabSelected(aDocContext, aVCContext) {
    // Send a pivot change message with the full context utterance for this doc.
    this.pivotChanged(aVCContext, Ci.nsIAccessiblePivot.REASON_NONE);
  },

  tabStateChanged: function AndroidPresenter_tabStateChanged(aDocObj,
                                                             aPageState) {
    this._appAnnounce(
      UtteranceGenerator.genForTabStateChange(aDocObj, aPageState));
  },

  textChanged: function AndroidPresenter_textChanged(aIsInserted, aStart,
                                                     aLength, aText,
                                                     aModifiedText) {
    let androidEvent = {
      type: 'Accessibility:Event',
      eventType: this.ANDROID_VIEW_TEXT_CHANGED,
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

    this.sendMessageToJava({gecko: androidEvent});
  },

  viewportChanged: function AndroidPresenter_viewportChanged() {
    if (Utils.AndroidSdkVersion < 14)
      return;

    let win = Utils.getBrowserApp(this.chromeWin).selectedBrowser.contentWindow;
    this.sendMessageToJava({
      gecko: {
        type: 'Accessibility:Event',
        eventType: this.ANDROID_VIEW_SCROLLED,
        text: [],
        scrollX: win.scrollX,
        scrollY: win.scrollY,
        maxScrollX: win.scrollMaxX,
        maxScrollY: win.scrollMaxY
      }
    });
  },

  editingModeChanged: function AndroidPresenter_editingModeChanged(aIsEditing) {
    this._appAnnounce(UtteranceGenerator.genForEditingMode(aIsEditing));
  },

  _appAnnounce: function _appAnnounce(aUtterance) {
    if (!aUtterance.length)
      return;

    this.sendMessageToJava({
      gecko: {
        type: 'Accessibility:Event',
        eventType: this.ANDROID_VIEW_TEXT_CHANGED,
        text: aUtterance,
        addedCount: aUtterance.join(' ').length,
        removedCount: 0,
        fromIndex: 0
      }
    });
  },

  sendMessageToJava: function AndroidPresenter_sendMessageTojava(aMessage) {
    return Cc['@mozilla.org/android/bridge;1'].
      getService(Ci.nsIAndroidBridge).
      handleGeckoMessage(JSON.stringify(aMessage));
  }
};

/**
 * A dummy Android presenter for desktop testing
 */

function DummyAndroidPresenter() {}

DummyAndroidPresenter.prototype = {
  __proto__: AndroidPresenter.prototype,

  sendMessageToJava: function DummyAndroidPresenter_sendMessageToJava(aMsg) {
    Logger.debug('Android event:\n' + JSON.stringify(aMsg, null, 2));
  }
};

/**
 * A speech presenter for direct TTS output
 */

function SpeechPresenter() {}

SpeechPresenter.prototype = {
  __proto__: Presenter.prototype,


  pivotChanged: function SpeechPresenter_pivotChanged(aContext, aReason) {
    if (!aContext.accessible)
      return;

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

    Logger.info('SPEAK', '"' + output.join(' ') + '"');
  }
}

/**
 * PresenterContext: An object that generates and caches context information
 * for a given accessible and its relationship with another accessible.
 */
function PresenterContext(aAccessible, aOldAccessible) {
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
