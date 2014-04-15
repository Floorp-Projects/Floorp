/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { data } = require("sdk/self");
var { ToggleButton } = require("sdk/ui");

var base64png = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYA" +
                "AABzenr0AAAASUlEQVRYhe3O0QkAIAwD0eyqe3Q993AQ3cBSUKpygfsNTy" +
                "N5ugbQpK0BAADgP0BRDWXWlwEAAAAAgPsA3rzDaAAAAHgPcGrpgAnzQ2FG" +
                "bWRR9AAAAABJRU5ErkJggg%3D%3D";

var reddit_panel = require("sdk/panel").Panel({
  width: 240,
  height: 320,
  contentURL: "http://www.reddit.com/.mobile?keep_extension=True",
  contentScriptFile: [data.url("jquery-1.4.4.min.js"),
                      data.url("panel.js")],
  onHide: handleHide
});

reddit_panel.port.on("click", function(url) {
  require("sdk/tabs").open(url);
});

let button = ToggleButton({
  id: "open-reddit-btn",
  label: "Reddit",
  icon: base64png,
  onChange: handleChange
});

exports.main = function(options, callbacks) {
  // If you run cfx with --static-args='{"quitWhenDone":true}' this program
  // will automatically quit Firefox when it's done.
  if (options.staticArgs.quitWhenDone)
    callbacks.quit();
};

function handleChange(state) {
  if (state.checked) {
    reddit_panel.show({ position: button });
  }
}

function handleHide() {
  button.state('window', { checked: false });
}
