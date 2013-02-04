/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "deprecated"
};

const {Cc,Ci,Cu,components} = require("chrome");
var trackedObjects = {};

var Compacter = {
  INTERVAL: 5000,
  notify: function(timer) {
    var newTrackedObjects = {};
    for (let name in trackedObjects) {
      var oldBin = trackedObjects[name];
      var newBin = [];
      var strongRefs = [];
      for (var i = 0; i < oldBin.length; i++) {
        var strongRef = oldBin[i].weakref.get();
        if (strongRef && strongRefs.indexOf(strongRef) == -1) {
          strongRefs.push(strongRef);
          newBin.push(oldBin[i]);
        }
      }
      if (newBin.length)
        newTrackedObjects[name] = newBin;
    }
    trackedObjects = newTrackedObjects;
  }
};

var timer = Cc["@mozilla.org/timer;1"]
            .createInstance(Ci.nsITimer);

timer.initWithCallback(Compacter,
                       Compacter.INTERVAL,
                       Ci.nsITimer.TYPE_REPEATING_SLACK);

var track = exports.track = function track(object, bin, stackFrameNumber) {
  var frame = components.stack.caller;
  var weakref = Cu.getWeakReference(object);
  if (!bin && 'constructor' in object)
    bin = object.constructor.name;
  if (bin == "Object")
    bin = frame.name;
  if (!bin)
    bin = "generic";
  if (!(bin in trackedObjects))
    trackedObjects[bin] = [];

  if (stackFrameNumber > 0)
    for (var i = 0; i < stackFrameNumber; i++)
      frame = frame.caller;

  trackedObjects[bin].push({weakref: weakref,
                            created: new Date(),
                            filename: frame.filename,
                            lineNo: frame.lineNumber,
                            bin: bin});
};

var getBins = exports.getBins = function getBins() {
  var names = [];
  for (let name in trackedObjects)
    names.push(name);
  return names;
};

var getObjects = exports.getObjects = function getObjects(bin) {
  function getLiveObjectsInBin(bin, array) {
    for (var i = 0; i < bin.length; i++) {
      var object = bin[i].weakref.get();
      if (object)
        array.push(bin[i]);
    }
  }

  var results = [];
  if (bin) {
    if (bin in trackedObjects)
      getLiveObjectsInBin(trackedObjects[bin], results);
  } else
    for (let name in trackedObjects)
      getLiveObjectsInBin(trackedObjects[name], results);
  return results;
};

var gc = exports.gc = function gc() {
  // Components.utils.forceGC() doesn't currently perform
  // cycle collection, which means that e.g. DOM elements
  // won't be collected by it. Fortunately, there are
  // other ways...

  var window = Cc["@mozilla.org/appshell/appShellService;1"]
               .getService(Ci.nsIAppShellService)
               .hiddenDOMWindow;
  var test_utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils);
  test_utils.garbageCollect();
  Compacter.notify();

  // Not sure why, but sometimes it appears that we don't get
  // them all with just one CC, so let's do it again.
  test_utils.garbageCollect();
};

require("../system/unload").when(
  function() {
    trackedObjects = {};
    if (timer) {
      timer.cancel();
      timer = null;
    }
  });
