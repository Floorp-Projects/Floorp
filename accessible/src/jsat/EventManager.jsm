/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import('resource://gre/modules/accessibility/Utils.jsm');
Cu.import('resource://gre/modules/accessibility/Presentation.jsm');
Cu.import('resource://gre/modules/accessibility/TraversalRules.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

this.EXPORTED_SYMBOLS = ['EventManager'];

this.EventManager = {
  editState: {},

  start: function start(aSendMsgFunc) {
    try {
      if (!this._started) {
        this.sendMsgFunc = aSendMsgFunc || function() {};

        Logger.info('EventManager.start', Utils.MozBuildApp);

        this._started = true;
        Services.obs.addObserver(this, 'accessible-event', false);
      }

      this.present(Presentation.tabStateChanged(null, 'newtab'));

    } catch (x) {
      Logger.error('Failed to start EventManager');
      Logger.logException(x);
    }
  },

  stop: function stop() {
    if (!this._started) {
      return;
    }
    Logger.info('EventManager.stop', Utils.MozBuildApp);
    Services.obs.removeObserver(this, 'accessible-event');
    this._started = false;
  },

  handleEvent: function handleEvent(aEvent) {
    try {
      switch (aEvent.type) {
      case 'DOMActivate':
      {
        let activatedAcc =
          Utils.AccRetrieval.getAccessibleFor(aEvent.originalTarget);
        let [state, extState] = Utils.getStates(activatedAcc);

        // Checkable objects will have a state changed event that we will use
        // instead of this hackish DOMActivate. We will also know the true
        // action that was taken.
        if (state & Ci.nsIAccessibleStates.STATE_CHECKABLE)
          return;

        this.present(Presentation.actionInvoked(activatedAcc, 'click'));
        break;
      }
      case 'scroll':
      case 'resize':
      {
        // the target could be an element, document or window
        let window = null;
        if (aEvent.target instanceof Ci.nsIDOMWindow)
          window = aEvent.target;
        else if (aEvent.target instanceof Ci.nsIDOMDocument)
          window = aEvent.target.defaultView;
        else if (aEvent.target instanceof Ci.nsIDOMElement)
          window = aEvent.target.ownerDocument.defaultView;
        this.present(Presentation.viewportChanged(window));
        break;
      }
      }
    } catch (x) {
      Logger.error('Error handling DOM event');
      Logger.logException(x);
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case 'accessible-event':
        var event;
        try {
          event = aSubject.QueryInterface(Ci.nsIAccessibleEvent);
          this.handleAccEvent(event);
        } catch (x) {
          Logger.error('Error handing accessible event');
          Logger.logException(x);
          return;
        }
    }
  },

  handleAccEvent: function handleAccEvent(aEvent) {
    if (Logger.logLevel >= Logger.DEBUG)
      Logger.debug('A11yEvent', Logger.eventToString(aEvent),
                   Logger.accessibleToString(aEvent.accessible));

    // Don't bother with non-content events in firefox.
    if (Utils.MozBuildApp == 'browser' &&
        aEvent.eventType != Ci.nsIAccessibleEvent.EVENT_VIRTUALCURSOR_CHANGED &&
        aEvent.accessibleDocument != Utils.CurrentContentDoc) {
      return;
    }

    switch (aEvent.eventType) {
      case Ci.nsIAccessibleEvent.EVENT_VIRTUALCURSOR_CHANGED:
      {
        let pivot = aEvent.accessible.
          QueryInterface(Ci.nsIAccessibleCursorable).virtualCursor;
        let position = pivot.position;
        if (position.role == Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME)
          break;
        let event = aEvent.
          QueryInterface(Ci.nsIAccessibleVirtualCursorChangeEvent);
        let reason = event.reason;

        if (this.editState.editing)
          aEvent.accessibleDocument.takeFocus();

        this.present(
          Presentation.pivotChanged(position, event.oldAccessible, reason));

        break;
      }
      case Ci.nsIAccessibleEvent.EVENT_STATE_CHANGE:
      {
        let event = aEvent.QueryInterface(Ci.nsIAccessibleStateChangeEvent);
        if (event.state == Ci.nsIAccessibleStates.STATE_CHECKED &&
            !(event.isExtraState)) {
          this.present(
            Presentation.
              actionInvoked(aEvent.accessible,
                            event.isEnabled ? 'check' : 'uncheck'));
        }
        break;
      }
      case Ci.nsIAccessibleEvent.EVENT_SCROLLING_START:
      {
        let vc = Utils.getVirtualCursor(aEvent.accessibleDocument);
        vc.moveNext(TraversalRules.Simple, aEvent.accessible, true);
        break;
      }
      case Ci.nsIAccessibleEvent.EVENT_TEXT_CARET_MOVED:
      {
        let acc = aEvent.accessible;
        let characterCount = acc.
          QueryInterface(Ci.nsIAccessibleText).characterCount;
        let caretOffset = aEvent.
          QueryInterface(Ci.nsIAccessibleCaretMoveEvent).caretOffset;

        // Update editing state, both for presenter and other things
        let [,extState] = Utils.getStates(acc);
        let editState = {
          editing: !!(extState & Ci.nsIAccessibleStates.EXT_STATE_EDITABLE),
          multiline: !!(extState & Ci.nsIAccessibleStates.EXT_STATE_MULTI_LINE),
          atStart: caretOffset == 0,
          atEnd: caretOffset == characterCount
        };

        // Not interesting
        if (!editState.editing && editState.editing == this.editState.editing)
          break;

        if (editState.editing != this.editState.editing)
          this.present(Presentation.editingModeChanged(editState.editing));

        if (editState.editing != this.editState.editing ||
            editState.multiline != this.editState.multiline ||
            editState.atEnd != this.editState.atEnd ||
            editState.atStart != this.editState.atStart)
          this.sendMsgFunc("AccessFu:Input", editState);

        this.editState = editState;
        break;
      }
      case Ci.nsIAccessibleEvent.EVENT_TEXT_INSERTED:
      case Ci.nsIAccessibleEvent.EVENT_TEXT_REMOVED:
      {
        if (aEvent.isFromUserInput) {
          // XXX support live regions as well.
          let event = aEvent.QueryInterface(Ci.nsIAccessibleTextChangeEvent);
          let isInserted = event.isInserted;
          let txtIface = aEvent.accessible.QueryInterface(Ci.nsIAccessibleText);

          let text = '';
          try {
            text = txtIface.
              getText(0, Ci.nsIAccessibleText.TEXT_OFFSET_END_OF_TEXT);
          } catch (x) {
            // XXX we might have gotten an exception with of a
            // zero-length text. If we did, ignore it (bug #749810).
            if (txtIface.characterCount)
              throw x;
          }
          this.present(Presentation.textChanged(
                         isInserted, event.start, event.length,
                         text, event.modifiedText));
        }
        break;
      }
      case Ci.nsIAccessibleEvent.EVENT_FOCUS:
      {
        // Put vc where the focus is at
        let acc = aEvent.accessible;
        let doc = aEvent.accessibleDocument;
        if (acc.role != Ci.nsIAccessibleRole.ROLE_DOCUMENT &&
            doc.role != Ci.nsIAccessibleRole.ROLE_CHROME_WINDOW) {
          let vc = Utils.getVirtualCursor(doc);
          vc.moveNext(TraversalRules.Simple, acc, true);
        }
        break;
      }
    }
  },

  present: function present(aPresentationData) {
    this.sendMsgFunc("AccessFu:Present", aPresentationData);
  },

  presentVirtualCursorPosition: function presentVirtualCursorPosition(aVirtualCursor) {
    this.present(Presentation.pivotChanged(aVirtualCursor.position, null,
                                           Ci.nsIAccessiblePivot.REASON_NONE));
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let tabstate = '';

    let loadingState = Ci.nsIWebProgressListener.STATE_TRANSFERRING |
      Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    let loadedState = Ci.nsIWebProgressListener.STATE_STOP |
      Ci.nsIWebProgressListener.STATE_IS_NETWORK;

    if ((aStateFlags & loadingState) == loadingState) {
      tabstate = 'loading';
    } else if ((aStateFlags & loadedState) == loadedState &&
               !aWebProgress.isLoadingDocument) {
      tabstate = 'loaded';
    }

    if (tabstate) {
      let docAcc = Utils.AccRetrieval.getAccessibleFor(aWebProgress.DOMWindow.document);
      this.present(Presentation.tabStateChanged(docAcc, tabstate));
    }
  },

  onProgressChange: function onProgressChange() {},

  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    let docAcc = Utils.AccRetrieval.getAccessibleFor(aWebProgress.DOMWindow.document);
    this.present(Presentation.tabStateChanged(docAcc, 'newdoc'));
  },

  onStatusChange: function onStatusChange() {},

  onSecurityChange: function onSecurityChange() {},

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports,
                                         Ci.nsIObserver])
};
