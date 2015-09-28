/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

this.EXPORTED_SYMBOLS = ['SystemAppProxy'];

var SystemAppProxy = {
  _frame: null,
  _isLoaded: false,
  _isReady: false,
  _pendingLoadedEvents: [],
  _pendingReadyEvents: [],
  _pendingListeners: [],

  // To call when a new system app iframe is created
  registerFrame: function (frame) {
    this._isReady = false;
    this._frame = frame;

    // Register all DOM event listeners added before we got a ref to the app iframe
    this._pendingListeners
        .forEach((args) =>
                 this.addEventListener.apply(this, args));
    this._pendingListeners = [];
  },

  // Get the system app frame
  getFrame: function () {
    return this._frame;
  },

  // To call when the load event of the System app document is triggered.
  // i.e. everything that is not lazily loaded are run and done.
  setIsLoaded: function () {
    if (this._isLoaded) {
      Cu.reportError('SystemApp has already been declared as being loaded.');
    }
    this._isLoaded = true;

    // Dispatch all events being queued while the system app was still loading
    this._pendingLoadedEvents
        .forEach(([type, details]) =>
                 this._sendCustomEvent(type, details, true));
    this._pendingLoadedEvents = [];
  },

  // To call when it is ready to receive events
  // i.e. when system-message-listener-ready mozContentEvent is sent.
  setIsReady: function () {
    if (!this._isLoaded) {
      Cu.reportError('SystemApp.setIsLoaded() should be called before setIsReady().');
    }

    if (this._isReady) {
      Cu.reportError('SystemApp has already been declared as being ready.');
    }
    this._isReady = true;

    // Dispatch all events being queued while the system app was still not ready
    this._pendingReadyEvents
        .forEach(([type, details]) =>
                 this._sendCustomEvent(type, details));
    this._pendingReadyEvents = [];
  },

  /*
   * Common way to send an event to the system app.
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
   *
   *   @returns event?  Dispatched event, or null if the event is pending.
   */
  _sendCustomEvent: function systemApp_sendCustomEvent(type,
                                                       details,
                                                       noPending,
                                                       target) {
    let content = this._frame ? this._frame.contentWindow : null;

    // If the system app isn't loaded yet,
    // queue events until someone calls setIsLoaded
    if (!content || !this._isLoaded) {
      if (noPending) {
        this._pendingLoadedEvents.push([type, details]);
      } else {
        this._pendingReadyEvents.push([type, details]);
      }

      return null;
    }

    // If the system app isn't ready yet,
    // queue events until someone calls setIsReady
    if (!this._isReady && !noPending) {
      this._pendingReadyEvents.push([type, details]);
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

    if ((target || content) === this._frame.contentWindow) {
      dump('XXX FIXME : Dispatch a ' + type + ': ' + details.type + "\n");
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
      let content = this._frame ? this._frame.contentWindow : null;
      if (!content) {
        throw new Error("no content window");
      }

      // If we don't already have a TextInputProcessor, create one now
      if (!this.TIP) {
        this.TIP = Cc["@mozilla.org/text-input-processor;1"]
          .createInstance(Ci.nsITextInputProcessor);
        if (!this.TIP) {
          throw new Error("failed to create textInputProcessor");
        }
      }

      if (!this.TIP.beginInputTransactionForTests(content)) {
        this.TIP = null;
        throw new Error("beginInputTransaction failed");
      }

      let e = new content.KeyboardEvent("", { key: details.key, });

      if (type === 'keydown') {
        this.TIP.keydown(e);
      }
      else if (type === 'keyup') {
        this.TIP.keyup(e);
      }
      else {
        throw new Error("unexpected event type: " + type);
      }
    }
    catch(e) {
      dump("dispatchKeyboardEvent: " + e + "\n");
    }
  },

  // Listen for dom events on the system app
  addEventListener: function systemApp_addEventListener() {
    let content = this._frame ? this._frame.contentWindow : null;
    if (!content) {
      this._pendingListeners.push(arguments);
      return false;
    }

    content.addEventListener.apply(content, arguments);
    return true;
  },

  removeEventListener: function systemApp_removeEventListener(name, listener) {
    let content = this._frame ? this._frame.contentWindow : null;
    if (content) {
      content.removeEventListener.apply(content, arguments);
    } else {
      this._pendingListeners = this._pendingListeners.filter(
        args => {
          return args[0] != name || args[1] != listener;
        });
    }
  },

  getFrames: function systemApp_getFrames() {
    let systemAppFrame = this._frame;
    if (!systemAppFrame) {
      return [];
    }
    let list = [systemAppFrame];
    let frames = systemAppFrame.contentDocument.querySelectorAll('iframe');
    for (let i = 0; i < frames.length; i++) {
      list.push(frames[i]);
    }
    return list;
  }
};
this.SystemAppProxy = SystemAppProxy;

