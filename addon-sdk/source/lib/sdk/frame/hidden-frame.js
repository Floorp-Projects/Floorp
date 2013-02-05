/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Cc, Ci } = require("chrome");
const errors = require("../deprecated/errors");
const { Class } = require("../core/heritage");
const { List, addListItem, removeListItem } = require("../util/list");
const { EventTarget } = require("../event/target");
const { emit } = require("../event/core");
const { create: makeFrame } = require("./utils");
const { defer, resolve } = require("../core/promise");
const { when: unload } = require("../system/unload");
const { validateOptions, getTypeOf } = require("../deprecated/api-utils");


let appShellService = Cc["@mozilla.org/appshell/appShellService;1"].
                        getService(Ci.nsIAppShellService);

let hiddenWindow = appShellService.hiddenDOMWindow;

if (!hiddenWindow) {
  throw new Error([
    "The hidden-frame module needs an app that supports a hidden window. ",
    "We would like it to support other applications, however. Please see ",
    "https://bugzilla.mozilla.org/show_bug.cgi?id=546740 for more information."
  ].join(""));
}

// This cache is used to access friend properties between functions
// without exposing them on the public API.
let cache = [];
let elements = new WeakMap();

function contentLoaded(target) {
  var deferred = defer();
  target.addEventListener("DOMContentLoaded", function DOMContentLoaded(event) {
    // "DOMContentLoaded" events from nested frames propagate up to target,
    // ignore events unless it's DOMContentLoaded for the given target.
    if (event.target === target || event.target === target.contentDocument) {
      target.removeEventListener("DOMContentLoaded", DOMContentLoaded, false);
      deferred.resolve(target);
    }
  }, false);
  return deferred.promise;
}

function makeHostFrame() {
  // Check if we can use the hidden window itself to host our iframes.
  // If it's not a suitable host, the hostFrame will be lazily created
  // by the first HiddenFrame instance.
  if (hiddenWindow.location.protocol == "chrome:" &&
      (hiddenWindow.document.contentType == "application/vnd.mozilla.xul+xml" ||
      hiddenWindow.document.contentType == "application/xhtml+xml")) {

    // Resolve to an object with a shape similar to iframe.
    return resolve({ contentDocument: hiddenWindow.document });
  }
  else {
    return contentLoaded(makeFrame(hiddenWindow.document, {
      // Ugly ugly hack. This is the most lightweight "chrome:" file I could
      // find on the tree.
      // This hack should be removed by proper platform support on bug 565388
      uri: "chrome://global/content/mozilla.xhtml",
      namespaceURI: hiddenWindow.document.documentElement.namespaceURI,
      nodeName: "iframe",
      allowJavascript: true,
      allowPlugins: true,
      allowAuth: true
    }));
  }
}
var hostFrame = makeHostFrame();

function FrameOptions(options) {
  options = options || {}
  return validateOptions(options, FrameOptions.validator);
}
FrameOptions.validator = {
  onReady: {
    is: ["undefined", "function", "array"],
    ok: function(v) {
      if (getTypeOf(v) === "array") {
        // make sure every item is a function
        return v.every(function (item) typeof(item) === "function")
      }
      return true;
    }
  },
  onUnload: {
    is: ["undefined", "function"]
  }
};

var HiddenFrame = Class({
  extends: EventTarget,
  initialize: function initialize(options) {
    options = FrameOptions(options);
    EventTarget.prototype.initialize.call(this, options);
  },
  get element() {
    return elements.get(this);
  },
  toString: function toString() {
    return "[object Frame]"
  }
});
exports.HiddenFrame = HiddenFrame

function isFrameCached(frame) {
  // Function returns `true` if frame was already cached.
  return cache.some(function(value) {
    return value === frame
  })
}

function addHidenFrame(frame) {
  if (!(frame instanceof HiddenFrame))
    throw Error("The object to be added must be a HiddenFrame.");

  // This instance was already added.
  if (isFrameCached(frame)) return frame;
  else cache.push(frame);

  hostFrame.then(function({ contentDocument }) {
    let element = makeFrame(contentDocument, {
      nodeName: "iframe",
      type: "content",
      allowJavascript: true,
      allowPlugins: true,
      allowAuth: true,
    });
    elements.set(frame, element);
    return contentLoaded(element);
  }).then(function onFrameReady(element) {
    emit(frame, "ready");
  }, console.exception);

  return frame;
}
exports.add = addHidenFrame

function removeHiddenFrame(frame) {
  if (!(frame instanceof HiddenFrame))
    throw Error("The object to be removed must be a HiddenFrame.");

  if (!isFrameCached(frame)) return;

  // Remove from cache before calling in order to avoid loop
  cache.splice(cache.indexOf(frame), 1);
  emit(frame, "unload")
  let element = frame.element
  if (element) element.parentNode.removeChild(element)
}
exports.remove = removeHiddenFrame;

unload(function () {
  cache.splice(0).forEach(removeHiddenFrame);

  hostFrame.then(function(host) {
    if (hast.parentNode) frame.parentNode.removeChild(frame);
  });
});
