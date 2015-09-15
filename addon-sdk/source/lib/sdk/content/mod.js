/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Ci } = require("chrome");
const { dispatcher } = require("../util/dispatcher");
const { add, remove, iterator } = require("../lang/weak-set");

var getTargetWindow = dispatcher("getTargetWindow");

getTargetWindow.define(function (target) {
  if (target instanceof Ci.nsIDOMWindow)
    return target;
  if (target instanceof Ci.nsIDOMDocument)
    return target.defaultView || null;

  return null;
});

exports.getTargetWindow = getTargetWindow;

var attachTo = dispatcher("attachTo");
exports.attachTo = attachTo;

var detachFrom = dispatcher("detatchFrom");
exports.detachFrom = detachFrom;

function attach(modification, target) {
  if (!modification)
    return;

  let window = getTargetWindow(target);

  attachTo(modification, window);

  // modification are stored per content; `window` reference can still be the
  // same even if the content is changed, therefore `document` is used instead.
  add(modification, window.document);
}
exports.attach = attach;

function detach(modification, target) {
  if (!modification)
    return;

  if (target) {
    let window = getTargetWindow(target);
    detachFrom(modification, window);
    remove(modification, window.document);
  }
  else {
    let documents = iterator(modification);
    for (let document of documents) {
      let window = document.defaultView;
      // The window might have already gone away
      if (!window)
        continue;
      detachFrom(modification, document.defaultView);
      remove(modification, document);
    }
  }
}
exports.detach = detach;
