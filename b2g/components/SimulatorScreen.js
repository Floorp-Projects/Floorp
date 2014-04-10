/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/DOMRequestHelper.jsm');

XPCOMUtils.defineLazyModuleGetter(this, 'GlobalSimulatorScreen',
                                  'resource://gre/modules/GlobalSimulatorScreen.jsm');

let DEBUG_PREFIX = 'SimulatorScreen.js - ';
function debug() {
  //dump(DEBUG_PREFIX + Array.slice(arguments) + '\n');
}

function fireOrientationEvent(window) {
  let e = new window.Event('mozorientationchange');
  window.screen.dispatchEvent(e);
}

function hookScreen(window) {
  let nodePrincipal = window.document.nodePrincipal;
  let origin = nodePrincipal.origin;
  if (nodePrincipal.appStatus == nodePrincipal.APP_STATUS_NOT_INSTALLED) {
    Cu.reportError('deny mozLockOrientation:' + origin + 'is not installed');
    return;
  }

  let screen = window.wrappedJSObject.screen;

  screen.mozLockOrientation = function (orientation) {
    debug('mozLockOrientation:', orientation, 'from', origin);

    // Normalize and do some checks against orientation input
    if (typeof(orientation) == 'string') {
      orientation = [orientation];
    }

    function isInvalidOrientationString(str) {
      return typeof(str) != 'string' ||
        !str.match(/^default$|^(portrait|landscape)(-(primary|secondary))?$/);
    }
    if (!Array.isArray(orientation) ||
        orientation.some(isInvalidOrientationString)) {
      Cu.reportError('Invalid orientation "' + orientation + '"');
      return false;
    }

    GlobalSimulatorScreen.lock(orientation);

    return true;
  };

  screen.mozUnlockOrientation = function() {
    debug('mozOrientationUnlock from', origin);
    GlobalSimulatorScreen.unlock();
    return true;
  };

  Object.defineProperty(screen, 'width', {
    get: function () GlobalSimulatorScreen.width
  });
  Object.defineProperty(screen, 'height', {
    get: function () GlobalSimulatorScreen.height
  });
  Object.defineProperty(screen, 'mozOrientation', {
    get: function () GlobalSimulatorScreen.mozOrientation
  });
}

function SimulatorScreen() {}
SimulatorScreen.prototype = {
  classID:         Components.ID('{c83c02c0-5d43-4e3e-987f-9173b313e880}'),
  QueryInterface:  XPCOMUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),
  _windows: new Map(),

  observe: function (subject, topic, data) {
    let windows = this._windows;
    switch (topic) {
      case 'profile-after-change':
        Services.obs.addObserver(this, 'document-element-inserted', false);
        Services.obs.addObserver(this, 'simulator-orientation-change', false);
        Services.obs.addObserver(this, 'inner-window-destroyed', false);
        break;

      case 'document-element-inserted':
        let window = subject.defaultView;
        if (!window) {
          return;
        }

        hookScreen(window);

        var id = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils)
                       .currentInnerWindowID;
        windows.set(id, window);
        break;

      case 'inner-window-destroyed':
        var id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
        windows.delete(id);
        break;

      case 'simulator-orientation-change':
        windows.forEach(fireOrientationEvent);
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SimulatorScreen]);
