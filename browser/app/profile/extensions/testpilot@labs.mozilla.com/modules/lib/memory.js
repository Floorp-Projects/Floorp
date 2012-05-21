/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var compactTimerId;
var COMPACT_INTERVAL = 5000;
var trackedObjects = {};

function scheduleNextCompaction() {
  compactTimerId = require("timer").setTimeout(compact, COMPACT_INTERVAL);
}

function compact() {
  var newTrackedObjects = {};
  for (name in trackedObjects) {
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
  scheduleNextCompaction();
}

var track = exports.track = function track(object, bin, stackFrameNumber) {
  if (!compactTimerId)
    scheduleNextCompaction();
  var frame = Components.stack.caller;
  var weakref = Cu.getWeakReference(object);
  if (!bin)
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
                            lineNo: frame.lineNumber});
};

var getBins = exports.getBins = function getBins() {
  var names = [];
  for (name in trackedObjects)
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
    for (name in trackedObjects)
      getLiveObjectsInBin(trackedObjects[name], results);
  return results;
};

require("unload").when(function() { trackedObjects = {}; });
