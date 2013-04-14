/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Ci, Cc } = require("chrome");
const { make: makeWindow, getHiddenWindow } = require("../window/utils");
const { create: makeFrame, getDocShell } = require("../frame/utils");
const { defer } = require("../core/promise");
const { when: unload } = require("../system/unload");

let addonPrincipal = Cc["@mozilla.org/systemprincipal;1"].
                     createInstance(Ci.nsIPrincipal);

// Once Bug 565388 is fixed and shipped we'll be able to make invisible,
// permanent docShells. Meanwhile we create hidden top level window and
// use it's docShell.
let frame = makeFrame(getHiddenWindow().document, {
  nodeName: "iframe",
  namespaceURI: "http://www.w3.org/1999/xhtml",
  allowJavascript: true,
  allowPlugins: true
})
let docShell = getDocShell(frame);
let eventTarget = docShell.chromeEventHandler;

// We need to grant docShell system principals in order to load XUL document
// from data URI into it.
docShell.createAboutBlankContentViewer(addonPrincipal);

// Get a reference to the DOM window of the given docShell and load
// such document into that would allow us to create XUL iframes, that
// are necessary for hidden frames etc..
let window = docShell.contentViewer.DOMDocument.defaultView;
window.location = "data:application/vnd.mozilla.xul+xml;charset=utf-8,<window/>";

// Create a promise that is delivered once add-on window is interactive,
// used by add-on runner to defer add-on loading until window is ready.
let { promise, resolve } = defer();
eventTarget.addEventListener("DOMContentLoaded", function handler(event) {
  eventTarget.removeEventListener("DOMContentLoaded", handler, false);
  resolve();
}, false);



exports.ready = promise;
exports.window = window;

// Still close window on unload to claim memory back early.
unload(function() {
  window.close()
  frame.parentNode.removeChild(frame);
});
