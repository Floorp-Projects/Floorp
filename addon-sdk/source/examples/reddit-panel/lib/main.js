/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var data = require("sdk/self").data;

var reddit_panel = require("sdk/panel").Panel({
  width: 240,
  height: 320,
  contentURL: "http://www.reddit.com/.mobile?keep_extension=True",
  contentScriptFile: [data.url("jquery-1.4.4.min.js"),
                      data.url("panel.js")]
});

reddit_panel.port.on("click", function(url) {
  require("sdk/tabs").open(url);
});

require("sdk/widget").Widget({
  id: "open-reddit-btn",
  label: "Reddit",
  contentURL: "http://www.reddit.com/static/favicon.ico",
  panel: reddit_panel
});

exports.main = function(options, callbacks) {
  // If you run cfx with --static-args='{"quitWhenDone":true}' this program
  // will automatically quit Firefox when it's done.
  if (options.staticArgs.quitWhenDone)
    callbacks.quit();
};
