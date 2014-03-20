/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ 'GlobalSimulatorScreen' ];

const Cu = Components.utils;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

this.GlobalSimulatorScreen = {
  mozOrientationLocked: false,

  // Actual orientation of apps
  mozOrientation: 'portrait',

  // The restricted list of actual orientation that can be used
  // if mozOrientationLocked is true
  lockedOrientation: [],

  // The faked screen orientation
  // if screenOrientation doesn't match mozOrientation due
  // to lockedOrientation restriction, the app will be displayed
  // on the side on desktop
  screenOrientation: 'portrait',

  // Updated by screen.js
  width: 0, height: 0,

  lock: function(orientation) {
    this.mozOrientationLocked = true;

    // Normalize to portrait or landscape,
    // i.e. the possible values of screenOrientation
    function normalize(str) {
      if (str.match(/^portrait/)) {
        return 'portrait';
      } else if (str.match(/^landscape/)) {
        return 'landscape';
      } else {
        return 'portrait';
      }
    }
    this.lockedOrientation = orientation.map(normalize);

    this.updateOrientation();
  },

  unlock: function() {
    this.mozOrientationLocked = false;
    this.updateOrientation();
  },

  updateOrientation: function () {
    let orientation = this.screenOrientation;

    // If the orientation is locked, we have to ensure ending up with a value
    // of lockedOrientation. If none of lockedOrientation values matches
    // the screen orientation we just choose the first locked orientation.
    // This will be the precise scenario where the app is displayed on the
    // side on desktop!
    if (this.mozOrientationLocked &&
        this.lockedOrientation.indexOf(this.screenOrientation) == -1) {
      orientation = this.lockedOrientation[0];
    }

    // If the actual orientation changed,
    // we have to fire mozorientation DOM events
    if (this.mozOrientation != orientation) {
      this.mozOrientation = orientation;

      // Notify each app screen object to fire the event
      Services.obs.notifyObservers(null, 'simulator-orientation-change', null);
    }

    // Finally, in any case, we update the window size and orientation
    // (Use wrappedJSObject trick to be able to pass a raw JS object)
    Services.obs.notifyObservers({wrappedJSObject:this}, 'simulator-adjust-window-size', null);
  },

  flipScreen: function() {
    if (this.screenOrientation == 'portrait') {
      this.screenOrientation = 'landscape';
    } else if (this.screenOrientation == 'landscape') {
      this.screenOrientation = 'portrait';
    }
    this.updateOrientation();
  }
}
