/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Ci = Components.interfaces;
const Cu = Components.utils;

const EVENT_VIRTUALCURSOR_CHANGED = Ci.nsIAccessibleEvent.EVENT_VIRTUALCURSOR_CHANGED;
const EVENT_STATE_CHANGE = Ci.nsIAccessibleEvent.EVENT_STATE_CHANGE;
const EVENT_SCROLLING_START = Ci.nsIAccessibleEvent.EVENT_SCROLLING_START;
const EVENT_TEXT_CARET_MOVED = Ci.nsIAccessibleEvent.EVENT_TEXT_CARET_MOVED;
const EVENT_TEXT_INSERTED = Ci.nsIAccessibleEvent.EVENT_TEXT_INSERTED;
const EVENT_TEXT_REMOVED = Ci.nsIAccessibleEvent.EVENT_TEXT_REMOVED;
const EVENT_FOCUS = Ci.nsIAccessibleEvent.EVENT_FOCUS;

const ROLE_INTERNAL_FRAME = Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME;
const ROLE_DOCUMENT = Ci.nsIAccessibleRole.ROLE_DOCUMENT;
const ROLE_CHROME_WINDOW = Ci.nsIAccessibleRole.ROLE_CHROME_WINDOW;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Services',
  'resource://gre/modules/Services.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Utils',
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Logger',
  'resource://gre/modules/accessibility/Utils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Presentation',
  'resource://gre/modules/accessibility/Presentation.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'TraversalRules',
  'resource://gre/modules/accessibility/TraversalRules.jsm');

this.EXPORTED_SYMBOLS = ['EventManager'];

this.EventManager = function EventManager(aContentScope) {
  this.contentScope = aContentScope;
  this.addEventListener = this.contentScope.addEventListener.bind(
    this.contentScope);
  this.removeEventListener = this.contentScope.removeEventListener.bind(
    this.contentScope);
  this.sendMsgFunc = this.contentScope.sendAsyncMessage.bind(
    this.contentScope);
  this.webProgress = this.contentScope.docShell.
    QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIWebProgress);
};

this.EventManager.prototype = {
  editState: {},

  start: function start() {
    try {
      if (!this._started) {
        Logger.info('EventManager.start', Utils.MozBuildApp);

        this._started = true;

        AccessibilityEventObserver.addListener(this);

        this.webProgress.addProgressListener(this,
          (Ci.nsIWebProgress.NOTIFY_STATE_ALL |
           Ci.nsIWebProgress.NOTIFY_LOCATION));
        this.addEventListener('scroll', this, true);
        this.addEventListener('resize', this, true);
        // XXX: Ideally this would be an a11y event. Bug #742280.
        this.addEventListener('DOMActivate', this, true);
      }
      this.present(Presentation.tabStateChanged(null, 'newtab'));

    } catch (x) {
      Logger.logException(x, 'Failed to start EventManager');
    }
  },

  // XXX: Stop is not called when the tab is closed (|TabClose| event is too
  // late). It is only called when the AccessFu is disabled explicitly.
  stop: function stop() {
    if (!this._started) {
      return;
    }
    Logger.info('EventManager.stop', Utils.MozBuildApp);
    AccessibilityEventObserver.removeListener(this);
    try {
      this.webProgress.removeProgressListener(this);
      this.removeEventListener('scroll', this, true);
      this.removeEventListener('resize', this, true);
      // XXX: Ideally this would be an a11y event. Bug #742280.
      this.removeEventListener('DOMActivate', this, true);
    } catch (x) {
      // contentScope is dead.
    } finally {
      this._started = false;
    }
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
      Logger.logException(x, 'Error handling DOM event');
    }
  },

  handleAccEvent: function handleAccEvent(aEvent) {
    if (Logger.logLevel >= Logger.DEBUG)
      Logger.debug('A11yEvent', Logger.eventToString(aEvent),
                   Logger.accessibleToString(aEvent.accessible));

    // Don't bother with non-content events in firefox.
    if (Utils.MozBuildApp == 'browser' &&
        aEvent.eventType != EVENT_VIRTUALCURSOR_CHANGED &&
        aEvent.accessibleDocument.docType == 'window') {
      return;
    }

    switch (aEvent.eventType) {
      case EVENT_VIRTUALCURSOR_CHANGED:
      {
        let pivot = aEvent.accessible.
          QueryInterface(Ci.nsIAccessibleDocument).virtualCursor;
        let position = pivot.position;
        if (position && position.role == ROLE_INTERNAL_FRAME)
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
      case EVENT_STATE_CHANGE:
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
      case EVENT_SCROLLING_START:
      {
        let vc = Utils.getVirtualCursor(aEvent.accessibleDocument);
        vc.moveNext(TraversalRules.Simple, aEvent.accessible, true);
        break;
      }
      case EVENT_TEXT_CARET_MOVED:
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

        this.present(Presentation.textSelectionChanged(acc.getText(0,-1),
                     caretOffset, caretOffset, 0, 0, aEvent.isFromUserInput));

        this.editState = editState;
        break;
      }
      case EVENT_TEXT_INSERTED:
      case EVENT_TEXT_REMOVED:
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
      case EVENT_FOCUS:
      {
        // Put vc where the focus is at
        let acc = aEvent.accessible;
        let doc = aEvent.accessibleDocument;
        if (acc.role != ROLE_DOCUMENT && doc.role != ROLE_CHROME_WINDOW) {
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

const AccessibilityEventObserver = {

  /**
   * A WeakMap containing [content, EventManager] pairs.
   */
  eventManagers: new WeakMap(),

  /**
   * A total number of registered eventManagers.
   */
  listenerCount: 0,

  /**
   * An indicator of an active 'accessible-event' observer.
   */
  started: false,

  /**
   * Start an AccessibilityEventObserver.
   */
  start: function start() {
    if (this.started || this.listenerCount === 0) {
      return;
    }
    Services.obs.addObserver(this, 'accessible-event', false);
    this.started = true;
  },

  /**
   * Stop an AccessibilityEventObserver.
   */
  stop: function stop() {
    if (!this.started) {
      return;
    }
    Services.obs.removeObserver(this, 'accessible-event');
    // Clean up all registered event managers.
    this.eventManagers.clear();
    this.listenerCount = 0;
    this.started = false;
  },

  /**
   * Register an EventManager and start listening to the
   * 'accessible-event' messages.
   *
   * @param aEventManager EventManager
   *        An EventManager object that was loaded into the specific content.
   */
  addListener: function addListener(aEventManager) {
    let content = aEventManager.contentScope.content;
    if (!this.eventManagers.has(content)) {
      this.listenerCount++;
    }
    this.eventManagers.set(content, aEventManager);
    // Since at least one EventManager was registered, start listening.
    Logger.debug('AccessibilityEventObserver.addListener. Total:',
      this.listenerCount);
    this.start();
  },

  /**
   * Unregister an EventManager and, optionally, stop listening to the
   * 'accessible-event' messages.
   *
   * @param aEventManager EventManager
   *        An EventManager object that was stopped in the specific content.
   */
  removeListener: function removeListener(aEventManager) {
    let content = aEventManager.contentScope.content;
    if (!this.eventManagers.delete(content)) {
      return;
    }
    this.listenerCount--;
    Logger.debug('AccessibilityEventObserver.removeListener. Total:',
      this.listenerCount);
    if (this.listenerCount === 0) {
      // If there are no EventManagers registered at the moment, stop listening
      // to the 'accessible-event' messages.
      this.stop();
    }
  },

  /**
   * Lookup an EventManager for a specific content. If the EventManager is not
   * found, walk up the hierarchy of parent windows.
   * @param content Window
   *        A content Window used to lookup the corresponding EventManager.
   */
  getListener: function getListener(content) {
    let eventManager = this.eventManagers.get(content);
    if (eventManager) {
      return eventManager;
    }
    let parent = content.parent;
    if (parent === content) {
      // There is no parent or the parent is of a different type.
      return null;
    }
    return this.getListener(parent);
  },

  /**
   * Handle the 'accessible-event' message.
   */
  observe: function observe(aSubject, aTopic, aData) {
    if (aTopic !== 'accessible-event') {
      return;
    }
    let event = aSubject.QueryInterface(Ci.nsIAccessibleEvent);
    if (!event.accessibleDocument) {
      Logger.warning(
        'AccessibilityEventObserver.observe: no accessible document:',
        Logger.eventToString(event), "accessible:",
        Logger.accessibleToString(event.accessible));
      return;
    }
    let content = event.accessibleDocument.window;
    // Match the content window to its EventManager.
    let eventManager = this.getListener(content);
    if (!eventManager || !eventManager._started) {
      if (Utils.MozBuildApp === 'browser' &&
          !(content instanceof Ci.nsIDOMChromeWindow)) {
        Logger.warning(
          'AccessibilityEventObserver.observe: ignored event:',
          Logger.eventToString(event), "accessible:",
          Logger.accessibleToString(event.accessible), "document:",
          Logger.accessibleToString(event.accessibleDocument));
      }
      return;
    }
    try {
      eventManager.handleAccEvent(event);
    } catch (x) {
      Logger.logException(x, 'Error handing accessible event');
    } finally {
      return;
    }
  }
};
