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

const ACCESSFU_DISABLE = 0;
const ACCESSFU_ENABLE = 1;
const ACCESSFU_AUTO = 2;

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
    if (this.chromeWin)
      // XXX: only supports attaching to one window now.
      throw new Error('Only one window could be attached to AccessFu');

    dump('AccessFu attach!! ' + Services.appinfo.OS + '\n');
    this.chromeWin = aWindow;
    this.presenters = [];

    this.prefsBranch = Cc['@mozilla.org/preferences-service;1']
      .getService(Ci.nsIPrefService).getBranch('accessibility.accessfu.');
    this.prefsBranch.addObserver('activate', this, false);

    let accessPref = ACCESSFU_DISABLE;
    try {
      accessPref = this.prefsBranch.getIntPref('activate');
    } catch (x) {
    }

    this._processPreferences(accessPref);
  },

  /**
   * Start AccessFu mode, this primarily means controlling the virtual cursor
   * with arrow keys.
   */
  _enable: function _enable() {
    if (this._enabled)
      return;
    this._enabled = true;

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
    this.chromeWin.addEventListener('focus', this, true);
  },

  /**
   * Disable AccessFu and return to default interaction mode.
   */
  _disable: function _disable() {
    if (!this._enabled)
      return;
    this._enabled = false;

    dump('AccessFu disable');

    this.presenters.forEach(function(p) { p.detach(); });
    this.presenters = [];

    VirtualCursorController.detach();

    Services.obs.removeObserver(this, 'accessible-event');
    this.chromeWin.removeEventListener('DOMActivate', this, true);
    this.chromeWin.removeEventListener('resize', this, true);
    this.chromeWin.removeEventListener('scroll', this, true);
    this.chromeWin.removeEventListener('TabOpen', this, true);
    this.chromeWin.removeEventListener('focus', this, true);
  },

  _processPreferences: function _processPreferences(aPref) {
    if (Services.appinfo.OS == 'Android') {
      if (aPref == ACCESSFU_AUTO) {
        if (!this._observingSystemSettings) {
          Services.obs.addObserver(this, 'Accessibility:Settings', false);
          this._observingSystemSettings = true;
        }
        Cc['@mozilla.org/android/bridge;1'].
          getService(Ci.nsIAndroidBridge).handleGeckoMessage(
            JSON.stringify({ gecko: { type: 'Accessibility:Ready' } }));
        return;
      }

      if (this._observingSystemSettings) {
        Services.obs.removeObserver(this, 'Accessibility:Settings');
        this._observingSystemSettings = false;
      }
    }

    if (aPref == ACCESSFU_ENABLE)
      this._enable();
    else
      this._disable();
  },

  addPresenter: function addPresenter(presenter) {
    this.presenters.push(presenter);
    presenter.attach(this.chromeWin);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'focus':
      {
        if (aEvent.target instanceof Ci.nsIDOMWindow) {
          let docAcc = getAccessible(aEvent.target.document);
          let docContext = new PresenterContext(docAcc, null);
          let cursorable = docAcc.QueryInterface(Ci.nsIAccessibleCursorable);
          let vcContext = new PresenterContext(
            (cursorable) ? cursorable.virtualCursor.position : null, null);
          this.presenters.forEach(
            function(p) { p.tabSelected(docContext, vcContext); });
        }
        break;
      }
      case 'TabOpen':
      {
        let browser = aEvent.target.linkedBrowser || aEvent.target;
        // Store the new browser node. We will need to check later when a new
        // content document is attached if it has been attached to this new tab.
        // If it has, than we will need to send a 'loading' message along with
        // the usual 'newdoc' to presenters.
        this._pendingDocuments[browser] = true;
        this.presenters.forEach(function(p) { p.tabStateChanged(null, 'newtab'); });
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
        this.presenters.forEach(function(p) { p.viewportChanged(); });
        break;
      }
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case 'Accessibility:Settings':
        if (JSON.parse(aData).enabled)
          this._enable();
        else
          this._disable();
        break;
      case 'nsPref:changed':
        if (aData == 'activate')
          this._processPreferences(this.prefsBranch.getIntPref('activate'));
        break;
      case 'accessible-event':
        let event;
        try {
          event = aSubject.QueryInterface(Ci.nsIAccessibleEvent);
          this._handleAccEvent(event);
        } catch (ex) {
          dump(ex);
          return;
        }
    }
  },

  _handleAccEvent: function _handleAccEvent(aEvent) {
    switch (aEvent.eventType) {
      case Ci.nsIAccessibleEvent.EVENT_VIRTUALCURSOR_CHANGED:
        {
          let pivot = aEvent.accessible.
            QueryInterface(Ci.nsIAccessibleCursorable).virtualCursor;
          let event = aEvent.
            QueryInterface(Ci.nsIAccessibleVirtualCursorChangeEvent);
          let position = pivot.position;
          let doc = aEvent.DOMNode;

          if (position && position.DOMNode &&
              doc instanceof Ci.nsIDOMDocument) {
            // Set the caret to the start of the pivot position, and move
            // the focus in the same manner as browse with caret mode.
            // This blurs the focus on the previous pivot position (if it
            // was activated), and keeps us in a predictable spot for tab
            // focus.
            let sel = doc.getSelection();
            sel.collapse(position.DOMNode, 0);
            Cc["@mozilla.org/focus-manager;1"]
              .getService(Ci.nsIFocusManager).moveFocus(
                doc.defaultView, null, Ci.nsIFocusManager.MOVEFOCUS_CARET, 0);
          }

          let presenterContext = new PresenterContext(pivot.position,
                                                      event.oldAccessible);
          this.presenters.forEach(
            function(p) { p.pivotChanged(presenterContext); });
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
          else if (event.state == Ci.nsIAccessibleStates.STATE_BUSY &&
                   !(event.isExtraState()) && event.isEnabled()) {
            let role = event.accessible.role;
            if ((role == Ci.nsIAccessibleRole.ROLE_DOCUMENT ||
                 role == Ci.nsIAccessibleRole.ROLE_APPLICATION)) {
              // An existing document has changed to state "busy", this means
              // something is loading. Send a 'loading' message to presenters.
              this.presenters.forEach(
                function(p) {
                  p.tabStateChanged(event.accessible, 'loading');
                }
              );
            }
          }
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_REORDER:
        {
          let acc = aEvent.accessible;
          if (acc.childCount) {
            let docAcc = acc.getChildAt(0);
            if (this._pendingDocuments[aEvent.DOMNode]) {
              // This is a document in a new tab. Check if it is
              // in a BUSY state (i.e. loading), and inform presenters.
              // We need to do this because a state change event will not be
              // fired when an object is created with the BUSY state.
              // If this is not a new tab, don't bother because we sent 'loading'
              // when the previous doc changed its state to BUSY.
              let state = {};
              docAcc.getState(state, {});
              if (state.value & Ci.nsIAccessibleStates.STATE_BUSY &&
                  this._isNotChromeDoc(docAcc))
                this.presenters.forEach(
                  function(p) { p.tabStateChanged(docAcc, 'loading'); }
                );
              delete this._pendingDocuments[aEvent.DOMNode];
            }
            if (this._isBrowserDoc(docAcc))
              // A new top-level content document has been attached
              this.presenters.forEach(
                function(p) { p.tabStateChanged(docAcc, 'newdoc'); }
              );
          }
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_COMPLETE:
        {
          if (this._isNotChromeDoc(aEvent.accessible)) {
            this.presenters.forEach(
              function(p) {
                p.tabStateChanged(aEvent.accessible, 'loaded');
              }
            );
          }
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_STOPPED:
        {
          this.presenters.forEach(
            function(p) {
              p.tabStateChanged(aEvent.accessible, 'loadstopped');
            }
          );
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_DOCUMENT_RELOAD:
        {
          this.presenters.forEach(
            function(p) {
              p.tabStateChanged(aEvent.accessible, 'reload');
            }
          );
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_TEXT_INSERTED:
      case Ci.nsIAccessibleEvent.EVENT_TEXT_REMOVED:
      {
        if (aEvent.isFromUserInput) {
          // XXX support live regions as well.
          let event = aEvent.QueryInterface(Ci.nsIAccessibleTextChangeEvent);
          let isInserted = event.isInserted();
          let textIface = aEvent.accessible.QueryInterface(Ci.nsIAccessibleText);

          let text = '';
          try {
            text = textIface.
              getText(0, Ci.nsIAccessibleText.TEXT_OFFSET_END_OF_TEXT);
          } catch (x) {
            // XXX we might have gotten an exception with of a
            // zero-length text. If we did, ignore it (bug #749810).
            if (textIface.characterCount)
              throw x;
          }

          this.presenters.forEach(
            function(p) {
              p.textChanged(isInserted, event.start, event.length, text, event.modifiedText);
            }
          );
        }
        break;
      }
      default:
        break;
    }
  },

  /**
   * Check if accessible is a top-level content document (i.e. a child of a XUL
   * browser node).
   * @param {nsIAccessible} aDocAcc the accessible to check.
   * @return {boolean} true if this is a top-level content document.
   */
  _isBrowserDoc: function _isBrowserDoc(aDocAcc) {
    let parent = aDocAcc.parent;
    if (!parent)
      return false;

    let domNode = parent.DOMNode;
    if (!domNode)
      return false;

    const ns = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';
    return (domNode.localName == 'browser' && domNode.namespaceURI == ns);
  },

  /**
   * Check if document is not a local "chrome" document, like about:home.
   * @param {nsIDOMDocument} aDocument the document to check.
   * @return {boolean} true if this is not a chrome document.
   */
  _isNotChromeDoc: function _isNotChromeDoc(aDocument) {
    let location = aDocument.DOMNode.location;
    if (!location)
      return false;

    return location.protocol != "about:";
  },

  // A hash of documents that don't yet have an accessible tree.
  _pendingDocuments: {},

  // So we don't enable/disable twice
  _enabled: false,

  // Observing accessibility settings
  _observingSystemSettings: false
};

function getAccessible(aNode) {
  try {
    return Cc['@mozilla.org/accessibleRetrieval;1'].
      getService(Ci.nsIAccessibleRetrieval).getAccessibleFor(aNode);
  } catch (e) {
    return null;
  }
}
