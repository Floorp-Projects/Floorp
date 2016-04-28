/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

this.EXPORTED_SYMBOLS = ['SystemAppProxy'];

const kMainSystemAppId = 'main';

var SystemAppProxy = {
  _frameInfoMap: new Map(),
  _pendingLoadedEvents: [],
  _pendingReadyEvents: [],
  _pendingListeners: [],

  // To call when a main system app iframe is created
  // Only used for main system app.
  registerFrame: function systemApp_registerFrame(frame) {
    this.registerFrameWithId(kMainSystemAppId, frame);
  },

  // To call when a new system(-remote) app iframe is created with ID
  registerFrameWithId: function systemApp_registerFrameWithId(frameId,
                                                              frame) {
    // - Frame ID of main system app is predefined as 'main'.
    // - Frame ID of system-remote app is defined themselves.
    //
    // frameInfo = {
    //    isReady: ...,
    //    isLoaded: ...,
    //    frame: ...
    // }

    let frameInfo = { frameId: frameId,
                      isReady: false,
                      isLoaded: false,
                      frame: frame };

    this._frameInfoMap.set(frameId, frameInfo);

    // Register all DOM event listeners added before we got a ref to
    // this system app iframe.
    this._pendingListeners
        .forEach(args => {
          if (args[0] === frameInfo.frameId) {
            this.addEventListenerWithId.apply(this, args);
          }
        });
    // Removed registered event listeners.
    this._pendingListeners =
      this._pendingListeners
          .filter(args => { return args[0] != frameInfo.frameId; });
  },

  unregisterFrameWithId: function systemApp_unregisterFrameWithId(frameId) {
    this._frameInfoMap.delete(frameId);
    // remove all pending event listener to the deleted system(-remote) app
    this._pendingListeners = this._pendingListeners.filter(
      args => { return args[0] != frameId; });
    this._pendingReadyEvents = this._pendingReadyEvents.filter(
        ([evtFrameId]) => { return evtFrameId != frameId });
    this._pendingLoadedEvents = this._pendingLoadedEvents.filter(
        ([evtFrameId]) => { return evtFrameId != frameId });
  },

  // Get the main system app frame
  _getMainSystemAppInfo: function systemApp_getMainSystemAppInfo() {
    return this._frameInfoMap.get(kMainSystemAppId);
  },

  // Get the main system app frame
  // Only used for the main system app.
  getFrame: function systemApp_getFrame() {
    return this.getFrameWithId(kMainSystemAppId);
  },

  // Get the frame of the specific system app
  getFrameWithId: function systemApp_getFrameWithId(frameId) {
    let frameInfo = this._frameInfoMap.get(frameId);

    if (!frameInfo) {
      throw new Error('no frame ID is ' + frameId);
    }
    if (!frameInfo.frame) {
      throw new Error('no content window');
    }
    return frameInfo.frame;
  },

  // To call when the load event of the main system app document is triggered.
  // i.e. everything that is not lazily loaded are run and done.
  // Only used for the main system app.
  setIsLoaded: function systemApp_setIsLoaded() {
    this.setIsLoadedWithId(kMainSystemAppId);
  },

  // To call when the load event of the specific system app document is
  // triggered. i.e. everything that is not lazily loaded are run and done.
  setIsLoadedWithId: function systemApp_setIsLoadedWithId(frameId) {
    let frameInfo = this._frameInfoMap.get(frameId);
    if (!frameInfo) {
      throw new Error('no frame ID is ' + frameId);
    }

    if (frameInfo.isLoaded) {
      if (frameInfo.frameId === kMainSystemAppId) {
        Cu.reportError('SystemApp has already been declared as being loaded.');
      }
      else {
        Cu.reportError('SystemRemoteApp (ID: ' + frameInfo.frameId + ') ' +
                       'has already been declared as being loaded.');
      }
    }

    frameInfo.isLoaded = true;

    // Dispatch all events being queued while the system app was still loading
    this._pendingLoadedEvents
        .forEach(([evtFrameId, evtType, evtDetails]) => {
          if (evtFrameId === frameInfo.frameId) {
            this.sendCustomEventWithId(evtFrameId, evtType, evtDetails, true);
          }
        });
    // Remove sent events.
    this._pendingLoadedEvents =
      this._pendingLoadedEvents
          .filter(([evtFrameId]) => { return evtFrameId != frameInfo.frameId });
  },

  // To call when the main system app is ready to receive events
  // i.e. when system-message-listener-ready mozContentEvent is sent.
  // Only used for the main system app.
  setIsReady: function systemApp_setIsReady() {
    this.setIsReadyWithId(kMainSystemAppId);
  },

  // To call when the specific system(-remote) app is ready to receive events
  // i.e. when system-message-listener-ready mozContentEvent is sent.
  setIsReadyWithId: function systemApp_setIsReadyWithId(frameId) {
    let frameInfo = this._frameInfoMap.get(frameId);
    if (!frameInfo) {
      throw new Error('no frame ID is ' + frameId);
    }

    if (!frameInfo.isLoaded) {
      Cu.reportError('SystemApp.setIsLoaded() should be called before setIsReady().');
    }

    if (frameInfo.isReady) {
      Cu.reportError('SystemApp has already been declared as being ready.');
    }

    frameInfo.isReady = true;

    // Dispatch all events being queued while the system app was still not ready
    this._pendingReadyEvents
        .forEach(([evtFrameId, evtType, evtDetails]) => {
          if (evtFrameId === frameInfo.frameId) {
            this.sendCustomEventWithId(evtFrameId, evtType, evtDetails);
          }
        });

    // Remove sent events.
    this._pendingReadyEvents =
      this._pendingReadyEvents
          .filter(([evtFrameId]) => { return evtFrameId != frameInfo.frameId });
  },

  /*
   * Common way to send an event to the main system app.
   * Only used for the main system app.
   *
   * // In gecko code:
   *   SystemAppProxy.sendCustomEvent('foo', { data: 'bar' });
   * // In system app:
   *   window.addEventListener('foo', function (event) {
   *     event.details == 'bar'
   *   });
   *
   *   @param type      The custom event type.
   *   @param details   The event details.
   *   @param noPending Set to true to emit this event even before the system
   *                    app is ready.
   *                    Event is always pending if the app is not loaded yet.
   *   @param target    The element who dispatch this event.
   *
   *   @returns event?  Dispatched event, or null if the event is pending.
   */
  _sendCustomEvent: function systemApp_sendCustomEvent(type,
                                                       details,
                                                       noPending,
                                                       target) {
    let args = Array.prototype.slice.call(arguments);
    return this.sendCustomEventWithId
               .apply(this, [kMainSystemAppId].concat(args));
  },

  /*
   * Common way to send an event to the specific system app.
   *
   * // In gecko code (send custom event from main system app):
   *   SystemAppProxy.sendCustomEventWithId('main', 'foo', { data: 'bar' });
   * // In system app:
   *   window.addEventListener('foo', function (event) {
   *     event.details == 'bar'
   *   });
   *
   *   @param frameId   Specify the system(-remote) app who dispatch this event.
   *   @param type      The custom event type.
   *   @param details   The event details.
   *   @param noPending Set to true to emit this event even before the system
   *                    app is ready.
   *                    Event is always pending if the app is not loaded yet.
   *   @param target    The element who dispatch this event.
   *
   *   @returns event?  Dispatched event, or null if the event is pending.
   */
  sendCustomEventWithId: function systemApp_sendCustomEventWithId(frameId,
                                                                  type,
                                                                  details,
                                                                  noPending,
                                                                  target) {
    let frameInfo = this._frameInfoMap.get(frameId);
    let content = (frameInfo && frameInfo.frame) ?
                  frameInfo.frame.contentWindow : null;
    // If the system app isn't loaded yet,
    // queue events until someone calls setIsLoaded
    if (!content || !(frameInfo && frameInfo.isLoaded)) {
      if (noPending) {
        this._pendingLoadedEvents.push([frameId, type, details]);
      } else {
        this._pendingReadyEvents.push([frameId, type, details]);
      }
      return null;
    }

    // If the system app isn't ready yet,
    // queue events until someone calls setIsReady
    if (!(frameInfo && frameInfo.isReady) && !noPending) {
      this._pendingReadyEvents.push([frameId, type, details]);
      return null;
    }

    let event = content.document.createEvent('CustomEvent');

    let payload;
    // If the root object already has __exposedProps__,
    // we consider the caller already wrapped (correctly) the object.
    if ('__exposedProps__' in details) {
      payload = details;
    } else {
      payload = details ? Cu.cloneInto(details, content) : {};
    }

    if ((target || content) === frameInfo.frame.contentWindow) {
      dump('XXX FIXME : Dispatch a ' + type + ': ' + details.type + '\n');
    }

    event.initCustomEvent(type, true, false, payload);
    (target || content).dispatchEvent(event);

    return event;
  },

  // Now deprecated, use sendCustomEvent with a custom event name
  dispatchEvent: function systemApp_dispatchEvent(details, target) {
    return this._sendCustomEvent('mozChromeEvent', details, false, target);
  },

  dispatchKeyboardEvent: function systemApp_dispatchKeyboardEvent(type, details) {
    try {
      let frameInfo = this._getMainSystemAppInfo();
      let content = (frameInfo && frameInfo.frame) ? frameInfo.frame.contentWindow
                                                   : null;
      if (!content) {
        throw new Error('no content window');
      }
      // If we don't already have a TextInputProcessor, create one now
      if (!this.TIP) {
        this.TIP = Cc['@mozilla.org/text-input-processor;1']
          .createInstance(Ci.nsITextInputProcessor);
        if (!this.TIP) {
          throw new Error('failed to create textInputProcessor');
        }
      }

      if (!this.TIP.beginInputTransactionForTests(content)) {
        this.TIP = null;
        throw new Error('beginInputTransaction failed');
      }

      let e = new content.KeyboardEvent('', { key: details.key, });

      if (type === 'keydown') {
        this.TIP.keydown(e);
      }
      else if (type === 'keyup') {
        this.TIP.keyup(e);
      }
      else {
        throw new Error('unexpected event type: ' + type);
      }
    }
    catch(e) {
      dump('dispatchKeyboardEvent: ' + e + '\n');
    }
  },

  // Listen for dom events on the main system app
  addEventListener: function systemApp_addEventListener() {
    let args = Array.prototype.slice.call(arguments);
    this.addEventListenerWithId.apply(this, [kMainSystemAppId].concat(args));
  },

  // Listen for dom events on the specific system app
  addEventListenerWithId: function systemApp_addEventListenerWithId(frameId,
                                                                    ...args) {
    let frameInfo = this._frameInfoMap.get(frameId);

    if (!frameInfo) {
      this._pendingListeners.push(arguments);
      return false;
    }

    let content = frameInfo.frame.contentWindow;
    content.addEventListener.apply(content, args);
    return true;
  },

  // remove the event listener from the main system app
  removeEventListener: function systemApp_removeEventListener(name, listener) {
    this.removeEventListenerWithId.apply(this, [kMainSystemAppId, name, listener]);
  },

  // remove the event listener from the specific system app
  removeEventListenerWithId: function systemApp_removeEventListenerWithId(frameId,
                                                                          name,
                                                                          listener) {
    let frameInfo = this._frameInfoMap.get(frameId);

    if (frameInfo) {
      let content = frameInfo.frame.contentWindow;
      content.removeEventListener.apply(content, [name, listener]);
    }
    else {
      this._pendingListeners = this._pendingListeners.filter(
        args => {
          return args[0] != frameId || args[1] != name || args[2] != listener;
        });
    }
  },

  // Get all frame in system app
  getFrames: function systemApp_getFrames(frameId) {
    let frameList = [];

    for (let frameId of this._frameInfoMap.keys()) {
      let frameInfo = this._frameInfoMap.get(frameId);
      let systemAppFrame = frameInfo.frame;
      let subFrames = systemAppFrame.contentDocument.querySelectorAll('iframe');
      frameList.push(systemAppFrame);
      for (let i = 0; i < subFrames.length; ++i) {
        frameList.push(subFrames[i]);
      }
    }
    return frameList;
  }
};
this.SystemAppProxy = SystemAppProxy;
