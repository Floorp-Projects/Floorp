/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let {classes: Cc, interfaces: Ci} = Components;
let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
            .getService(Ci.mozIJSSubScriptLoader);
let EventUtils = {};
loader.loadSubScript("chrome://marionette/content/EventUtils.js", EventUtils);

function sendTestEvent(message) {
  let type = message.data;

  let divone = content.document.getElementById("one");
  let divtwo = content.document.getElementById("two");

  divone.addEventListener(type, function(event) {
    if (event.isSynthesized) {
      sendAsyncMessage("Test:EventReply");
    }
  });

  EventUtils.synthesizeMouse(divtwo, 5, 5, {type: "mousemove"}, divtwo.ownerGlobal);
  if (type === "click") {
    EventUtils.synthesizeMouse(divone, 5, 5, {}, divone.ownerGlobal);
  } else {
    EventUtils.synthesizeMouse(divone, 5, 5, {type: type}, divone.ownerGlobal);
  }
}

addMessageListener("Test:SynthesizeEvent", sendTestEvent);
