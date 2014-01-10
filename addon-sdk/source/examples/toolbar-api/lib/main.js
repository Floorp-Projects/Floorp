/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Toolbar } = require("sdk/ui/toolbar");
const { Frame } = require("sdk/ui/frame");
const { ActionButton } = require("sdk/ui/button/action");

let button = new ActionButton({
  id: "button",
  label: "send!",
  icon: "./favicon.ico",
  onClick: () => {
    frame.postMessage({
      hello: "content"
    });
  }
});

let frame = new Frame({
    url: "./index.html",
    onAttach: () => {
      console.log("frame was attached");
    },
    onReady: () => {
      console.log("frame document was loaded");
    },
    onLoad: () => {
      console.log("frame load complete");
    },
    onMessage: (event) => {
      console.log("got message from frame content", event);
      if (event.data === "ping!")
        event.source.postMessage("pong!", event.source.origin);
    }
});
let toolbar = new Toolbar({
  items: [frame],
  title: "Addon Demo",
  hidden: false,
  onShow: () => {
    console.log("toolbar was shown");
  },
  onHide: () => {
    console.log("toolbar was hidden");
  }
});
