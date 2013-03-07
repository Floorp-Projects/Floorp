/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};

const privateNS = require('../../core/namespace').ns();

function getOwnerWindow(thing) {
  try {
    // check for and return associated window
    let fn = (privateNS(thing.prototype) || privateNS(thing) || {}).getOwnerWindow;

    if (fn)
      return fn.apply(fn, [thing].concat(arguments));
  }
  // stuff like numbers and strings throw errors with namespaces
  catch(e) {}
  // default
  return undefined;
}
getOwnerWindow.define = function(Type, fn) {
  privateNS(Type.prototype).getOwnerWindow = fn;
}

getOwnerWindow.implement = function(instance, fn) {
  privateNS(instance).getOwnerWindow = fn;
}

exports.getOwnerWindow = getOwnerWindow;
