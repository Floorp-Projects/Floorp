/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Parts of this module were taken from narwhal:
//
// http://narwhaljs.org

module.metadata = {
  "stability": "experimental"
};

const { on, off } = require('./events');
const unloadSubject = require('@loader/unload');

const observers = [];
const unloaders = [];

var when = exports.when = function when(observer) {
  if (observers.indexOf(observer) != -1)
    return;
  observers.unshift(observer);
};

var ensure = exports.ensure = function ensure(obj, destructorName) {
  if (!destructorName)
    destructorName = "unload";
  if (!(destructorName in obj))
    throw new Error("object has no '" + destructorName + "' property");

  let called = false;
  let originalDestructor = obj[destructorName];

  function unloadWrapper(reason) {
    if (!called) {
      called = true;
      let index = unloaders.indexOf(unloadWrapper);
      if (index == -1)
        throw new Error("internal error: unloader not found");
      unloaders.splice(index, 1);
      originalDestructor.call(obj, reason);
      originalDestructor = null;
      destructorName = null;
      obj = null;
    }
  };

  // TODO: Find out why the order is inverted here. It seems that
  // it may be causing issues!
  unloaders.push(unloadWrapper);

  obj[destructorName] = unloadWrapper;
};

function unload(reason) {
  observers.forEach(function(observer) {
    try {
      observer(reason);
    }
    catch (error) {
      console.exception(error);
    }
  });
}

when(function(reason) {
  unloaders.slice().forEach(function(unloadWrapper) {
    unloadWrapper(reason);
  });
});

on('sdk:loader:destroy', function onunload({ subject, data: reason }) {
  // If this loader is unload then `subject.wrappedJSObject` will be
  // `destructor`.
  if (subject.wrappedJSObject === unloadSubject) {
    off('sdk:loader:destroy', onunload);
    unload(reason);
  }
// Note that we use strong reference to listener here to make sure it's not
// GC-ed, which may happen otherwise since nothing keeps reference to `onunolad`
// function.
}, true);
