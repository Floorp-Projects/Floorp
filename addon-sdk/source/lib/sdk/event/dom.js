/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Ci } = require("chrome");

let { emit } = require("./core");
let { when: unload } = require("../system/unload");
let listeners = new Map();

let getWindowFrom = x =>
                    x instanceof Ci.nsIDOMWindow ? x :
                    x instanceof Ci.nsIDOMDocument ? x.defaultView :
                    x instanceof Ci.nsIDOMNode ? x.ownerDocument.defaultView :
                    null;

function removeFromListeners() {
  listeners.delete(this);
}

// Simple utility function takes event target, event type and optional
// `options.capture` and returns node style event stream that emits "data"
// events every time event of that type occurs on the given `target`.
function open(target, type, options) {
  let output = {};
  let capture = options && options.capture ? true : false;
  let listener = (event) => emit(output, "data", event);

  // `open` is currently used only on DOM Window objects, however it was made
  // to be used to any kind of `target` that supports `addEventListener`,
  // therefore is safer get the `window` from the `target` instead assuming
  // that `target` is the `window`.
  let window = getWindowFrom(target);

  // If we're not able to get a `window` from `target`, there is something
  // wrong. We cannot add listeners that can leak later, or results in
  // "dead object" exception.
  // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1001833
  if (!window)
    throw new Error("Unable to obtain the owner window from the target given.");

  let cleaners = listeners.get(window) || [];
  cleaners.push(() => target.removeEventListener(type, listener, capture));

  listeners.set(window, cleaners);

  // We need to remove from our map the `window` once is closed, to prevent
  // memory leak
  window.addEventListener("DOMWindowClose", removeFromListeners);

  target.addEventListener(type, listener, capture);

  return output;
}

unload(() => {
  for (let [window, cleaners] of listeners) {
    cleaners.forEach(callback => callback())
  }

  listeners.clear();
});

exports.open = open;
