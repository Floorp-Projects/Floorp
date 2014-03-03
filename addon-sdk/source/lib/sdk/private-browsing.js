/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "stable"
};

const { Ci } = require('chrome');
const { setMode, getMode, on: onStateChange, isPermanentPrivateBrowsing } = require('./private-browsing/utils');
const { isWindowPrivate } = require('./window/utils');
const { emit, on, once, off } = require('./event/core');
const { when: unload } = require('./system/unload');
const { deprecateUsage, deprecateFunction, deprecateEvent } = require('./util/deprecate');
const { getOwnerWindow } = require('./private-browsing/window/utils');

onStateChange('start', function onStart() {
  emit(exports, 'start');
});

onStateChange('stop', function onStop() {
  emit(exports, 'stop');
});

Object.defineProperty(exports, "isActive", {
  get: deprecateFunction(getMode, 'require("private-browsing").isActive is deprecated.')
});

exports.activate = function activate() setMode(true);
exports.deactivate = function deactivate() setMode(false);

exports.on = deprecateEvents(on.bind(null, exports));
exports.once = deprecateEvents(once.bind(null, exports));
exports.removeListener = deprecateEvents(function removeListener(type, listener) {
  // Note: We can't just bind `off` as we do it for other methods cause skipping
  // a listener argument will remove all listeners for the given event type
  // causing misbehavior. This way we make sure all arguments are passed.
  off(exports, type, listener);
});

exports.isPrivate = function(thing) {
  // if thing is defined, and we can find a window for it
  // then check if the window is private
  if (!!thing) {
    // if the thing is a window, and the window is private
    // then return true
    if (isWindowPrivate(thing)) {
      return true;
    }

    // does the thing have an associated tab?
    // page-mod instances do..
    if (thing.tab) {
      let tabWindow = getOwnerWindow(thing.tab);
      if (tabWindow) {
        let isThingPrivate = isWindowPrivate(tabWindow);
        if (isThingPrivate)
          return isThingPrivate;
      }
    }

    // can we find an associated window?
    let window = getOwnerWindow(thing);
    if (window)
      return isWindowPrivate(window);

    try {
      let { isChannelPrivate } = thing.QueryInterface(Ci.nsIPrivateBrowsingChannel);
      if (isChannelPrivate)
        return true;
    } catch(e) {}
  }

  // check if the post pwpb, global pb service is enabled.
  if (isPermanentPrivateBrowsing())
    return true;

  // if we get here, and global private browsing
  // is available, and it is true, then return
  // true otherwise false is returned here
  return getMode();
};

function deprecateEvents(func) deprecateEvent(
  func,
   'The require("sdk/private-browsing") module\'s "start" and "stop" events ' +
   'are deprecated.',
  ['start', 'stop']
);

// Make sure listeners are cleaned up.
unload(function() off(exports));
