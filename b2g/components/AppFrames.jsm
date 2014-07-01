/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

this.EXPORTED_SYMBOLS = ['AppFrames'];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/SystemAppProxy.jsm');

const listeners = [];

const Observer = {
  // Save a map of (MessageManager => Frame) to be able to dispatch
  // the FrameDestroyed event with a frame reference.
  _frames: new Map(),

  // Also save current number of iframes opened by app
  _apps: new Map(),

  start: function () {
    Services.obs.addObserver(this, 'remote-browser-shown', false);
    Services.obs.addObserver(this, 'inprocess-browser-shown', false);
    Services.obs.addObserver(this, 'message-manager-disconnect', false);

    SystemAppProxy.getAppFrames().forEach((frame) => {
      let mm = frame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
      this._frames.set(mm, frame);
      let mozapp = frame.getAttribute('mozapp');
      this._apps.set(mozapp, (this._apps.get(mozapp) || 0) + 1);
    });
  },

  stop: function () {
    Services.obs.removeObserver(this, 'remote-browser-shown');
    Services.obs.removeObserver(this, 'inprocess-browser-shown');
    Services.obs.removeObserver(this, 'message-manager-disconnect');
    this._frames.clear();
    this._apps.clear();
  },

  observe: function (subject, topic, data) {
    switch(topic) {

      // Listen for frame creation in OOP (device) as well as in parent process (b2g desktop)
      case 'remote-browser-shown':
      case 'inprocess-browser-shown':
        let frameLoader = subject;

        // get a ref to the app <iframe>
        frameLoader.QueryInterface(Ci.nsIFrameLoader);
        let frame = frameLoader.ownerElement;
        let mm = frame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
        this.onMessageManagerCreated(mm, frame);
        break;

      // Every time an iframe is destroyed, its message manager also is
      case 'message-manager-disconnect':
        this.onMessageManagerDestroyed(subject);
        break;
    }
  },

  onMessageManagerCreated: function (mm, frame) {
    this._frames.set(mm, frame);

    let mozapp = frame.getAttribute('mozapp');
    let count = (this._apps.get(mozapp) || 0) + 1;
    this._apps.set(mozapp, count);

    let isFirstAppFrame = (count === 1);
    listeners.forEach(function (listener) {
      try {
        listener.onAppFrameCreated(frame, isFirstAppFrame);
      } catch(e) {
        dump('Exception while calling Frames.jsm listener:' + e + '\n' + e.stack + '\n');
      }
    });
  },

  onMessageManagerDestroyed: function (mm) {
    let frame = this._frames.get(mm);
    if (!frame) {
      // We receive an event for a non mozapp message manager
      return;
    }

    this._frames.delete(mm);

    let mozapp = frame.getAttribute('mozapp');
    let count = (this._apps.get(mozapp) || 0) - 1;
    this._apps.set(mozapp, count);

    let isLastAppFrame = (count === 0);
    listeners.forEach(function (listener) {
      try {
        listener.onAppFrameDestroyed(frame, isLastAppFrame);
      } catch(e) {
        dump('Exception while calling Frames.jsm listener:' + e + '\n' + e.stack + '\n');
      }
    });
  }

};

const AppFrames = this.AppFrames = {

  list: () => SystemAppProxy.getAppFrames(),

  addObserver: function (listener) {
    if (listeners.indexOf(listener) !== -1) {
      return;
    }

    listeners.push(listener);
    if (listeners.length == 1) {
      Observer.start();
    }
  },

  removeObserver: function (listener) {
    let idx = listeners.indexOf(listener);
    if (idx !== -1) {
      listeners.splice(idx, 1);
    }
    if (listeners.length === 0) {
      Observer.stop();
    }
  }

};

