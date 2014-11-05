/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: This code was forked from:
// https://github.com/mozilla-b2g/gaia/blob/master/tests/atoms/gaia_lock_screen.js

'use strict';

var GaiaLockScreen = {

  unlock: function() {
    let lockscreen = window.wrappedJSObject.lockScreen;
    let setlock = window.wrappedJSObject.SettingsListener.getSettingsLock();
    let system = window.wrappedJSObject.System;
    let obj = {'screen.timeout': 0};
    setlock.set(obj);

    window.wrappedJSObject.ScreenManager.turnScreenOn();

    waitFor(
      function() {
        window.wrappedJSObject.dispatchEvent(
          new window.wrappedJSObject.CustomEvent(
            'lockscreen-request-unlock', {
              detail: {
                forcibly: true
              }
            }));
        waitFor(
          function() {
            finish(system.locked);
          },
          function() {
            return !system.locked;
          }
        );
      },
      function() {
        return !!lockscreen;
      }
    );
  },

  lock: function() {
    let lwm = window.wrappedJSObject.lockScreenWindowManager;
    let lockscreen = window.wrappedJSObject.lockScreen;
    let system = window.wrappedJSObject.System;
    let setlock = window.wrappedJSObject.SettingsListener.getSettingsLock();
    let obj = {'screen.timeout': 0};
    let waitLock = function() {
      waitFor(
        function() {
        window.wrappedJSObject.dispatchEvent(
          new window.wrappedJSObject.CustomEvent(
            'lockscreen-request-lock', {
              detail: {
                forcibly: true
              }
            }));
          waitFor(
            function() {
              finish(!system.locked);
            },
            function() {
              return system.locked;
            }
          );
        },
        function() {
          return !!lockscreen;
        }
      );
    };

    setlock.set(obj);
    window.wrappedJSObject.ScreenManager.turnScreenOn();

    // Need to open the window before we lock the lockscreen.
    // This would only happen when someone directly call the lockscrene.lock.
    // It's a bad pattern and would only for test.
    lwm.openApp();
    waitFor(function() {
      waitLock();
    }, function() {
      return lwm.states.instance.isActive();
    });
  }
};

