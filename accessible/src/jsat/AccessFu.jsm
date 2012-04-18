/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

var EXPORTED_SYMBOLS = ['AccessFu'];

Cu.import('resource://gre/modules/Services.jsm');

Cu.import('resource://gre/modules/accessibility/Presenters.jsm');
Cu.import('resource://gre/modules/accessibility/VirtualCursorController.jsm');

var AccessFu = {
  /**
   * Attach chrome-layer accessibility functionality to the given chrome window.
   * If accessibility is enabled on the platform (currently Android-only), then
   * a special accessibility mode is started (see startup()).
   * @param {ChromeWindow} aWindow Chrome window to attach to.
   * @param {boolean} aForceEnabled Skip platform accessibility check and enable
   *  AccessFu.
   */
  attach: function attach(aWindow) {
    dump('AccessFu attach!! ' + Services.appinfo.OS + '\n');
    this.chromeWin = aWindow;
    this.presenters = [];

    function checkA11y() {
      if (Services.appinfo.OS == 'Android') {
        let msg = Cc['@mozilla.org/android/bridge;1'].
          getService(Ci.nsIAndroidBridge).handleGeckoMessage(
            JSON.stringify(
                { gecko: {
                    type: 'Accessibility:IsEnabled',
                    eventType: 1,
                    text: []
                  }
                }));
        return JSON.parse(msg).enabled;
      }
      return false;
    }

    if (checkA11y())
      this.enable();
  },

  /**
   * Start the special AccessFu mode, this primarily means controlling the virtual
   * cursor with arrow keys. Currently, on platforms other than Android this needs
   * to be called explicitly.
   */
  enable: function enable() {
    dump('AccessFu enable');
    this.addPresenter(new VisualPresenter());

    // Implicitly add the Android presenter on Android.
    if (Services.appinfo.OS == 'Android')
      this.addPresenter(new AndroidPresenter());

    VirtualCursorController.attach(this.chromeWin);

    Services.obs.addObserver(this, 'accessible-event', false);
    this.chromeWin.addEventListener('DOMActivate', this, true);
    this.chromeWin.addEventListener('resize', this, true);
    this.chromeWin.addEventListener('scroll', this, true);
    this.chromeWin.addEventListener('TabOpen', this, true);
    this.chromeWin.addEventListener('TabSelect', this, true);
    this.chromeWin.addEventListener('TabClosed', this, true);
  },

  /**
   * Disable AccessFu and return to default interaction mode.
   */
  disable: function disable() {
    dump('AccessFu disable');

    this.presenters.forEach(function(p) {p.detach();});
    this.presenters = [];

    VirtualCursorController.detach();

    Services.obs.addObserver(this, 'accessible-event', false);
    this.chromeWin.removeEventListener('DOMActivate', this);
    this.chromeWin.removeEventListener('resize', this);
    this.chromeWin.removeEventListener('scroll', this);
    this.chromeWin.removeEventListener('TabOpen', this);
    this.chromeWin.removeEventListener('TabSelect', this);
    this.chromeWin.removeEventListener('TabClose', this);
  },

  addPresenter: function addPresenter(presenter) {
    this.presenters.push(presenter);
    presenter.attach(this.chromeWin);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'TabSelect':
        {
          this.getDocAccessible(
              function(docAcc) {
                this.presenters.forEach(function(p) {p.tabSelected(docAcc);});
              });
          break;
        }
      case 'DOMActivate':
      {
        let activatedAcc = getAccessible(aEvent.originalTarget);
        let state = {};
        activatedAcc.getState(state, {});

        // Checkable objects will have a state changed event that we will use
        // instead of this hackish DOMActivate. We will also know the true
        // action that was taken.
        if (state.value & Ci.nsIAccessibleStates.STATE_CHECKABLE)
          return;

        this.presenters.forEach(function(p) {
                                  p.actionInvoked(activatedAcc, 'click');
                                });
        break;
      }
      case 'scroll':
      case 'resize':
      {
        this.presenters.forEach(function(p) {p.viewportChanged();});
        break;
      }
    }
  },

  getDocAccessible: function getDocAccessible(aCallback) {
    let browserApp = (Services.appinfo.OS == 'Android') ?
      this.chromeWin.BrowserApp : this.chromeWin.gBrowser;

    let docAcc = getAccessible(browserApp.selectedBrowser.contentDocument);
    if (!docAcc) {
      // Wait for a reorder event fired by the parent of the new doc.
      this._pendingDocuments[browserApp.selectedBrowser] = aCallback;
    } else {
      aCallback.apply(this, [docAcc]);
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case 'accessible-event':
        let event;
        try {
          event = aSubject.QueryInterface(Ci.nsIAccessibleEvent);
          this.handleAccEvent(event);
        } catch (ex) {
          dump(ex);
          return;
        }
    }
  },

  handleAccEvent: function handleAccEvent(aEvent) {
    switch (aEvent.eventType) {
      case Ci.nsIAccessibleEvent.EVENT_VIRTUALCURSOR_CHANGED:
        {
          let pivot = aEvent.accessible.
            QueryInterface(Ci.nsIAccessibleCursorable).virtualCursor;
          let event = aEvent.
            QueryInterface(Ci.nsIAccessibleVirtualCursorChangeEvent);

          let newContext = this.getNewContext(event.oldAccessible,
                                              pivot.position);
          this.presenters.forEach(
            function(p) {
              p.pivotChanged(pivot.position, newContext);
            });
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_STATE_CHANGE:
        {
          let event = aEvent.QueryInterface(Ci.nsIAccessibleStateChangeEvent);
          if (event.state == Ci.nsIAccessibleStates.STATE_CHECKED &&
              !(event.isExtraState())) {
            this.presenters.forEach(
              function(p) {
                p.actionInvoked(aEvent.accessible,
                                event.isEnabled() ? 'check' : 'uncheck');
              }
            );
          }
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_REORDER:
        {
          let node = aEvent.accessible.DOMNode;
          let callback = this._pendingDocuments[node];
          if (callback && aEvent.accessible.childCount) {
            // We have a callback associated with a document.
            callback.apply(this, [aEvent.accessible.getChildAt(0)]);
            delete this._pendingDocuments[node];
          }
          break;
        }
      default:
        break;
    }
  },

  getNewContext: function getNewContext(aOldObject, aNewObject) {
    let newLineage = [];
    let oldLineage = [];

    let parent = aNewObject;
    while ((parent = parent.parent))
      newLineage.push(parent);

    if (aOldObject) {
      parent = aOldObject;
      while ((parent = parent.parent))
        oldLineage.push(parent);
    }

//    newLineage.reverse();
//    oldLineage.reverse();

    let i = 0;
    let newContext = [];

    while (true) {
      let newAncestor = newLineage.pop();
      let oldAncestor = oldLineage.pop();

      if (newAncestor == undefined)
        break;

      if (newAncestor != oldAncestor)
        newContext.push(newAncestor);
      i++;
    }

    return newContext;
  },

  // A hash of documents that don't yet have an accessible tree.
  _pendingDocuments: {}
};

function getAccessible(aNode) {
  try {
    return Cc['@mozilla.org/accessibleRetrieval;1'].
      getService(Ci.nsIAccessibleRetrieval).getAccessibleFor(aNode);
  } catch (e) {
    return null;
  }
}
