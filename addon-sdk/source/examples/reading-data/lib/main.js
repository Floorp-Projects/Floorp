/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var self = require("sdk/self");
var panels = require("sdk/panel");
var widgets = require("sdk/widget");

function replaceMom(html) {
  return html.replace("World", "Mom");
}
exports.replaceMom = replaceMom;

exports.main = function(options, callbacks) {
  console.log("My ID is " + self.id);

  // Load the sample HTML into a string.
  var helloHTML = self.data.load("sample.html");

  // Let's now modify it...
  helloHTML = replaceMom(helloHTML);

  // ... and then create a panel that displays it.
  var myPanel = panels.Panel({
    contentURL: "data:text/html," + helloHTML
  });

  // Load the URL of the sample image.
  var iconURL = self.data.url("mom.png");

  // Create a widget that displays the image.  We'll attach the panel to it.
  // When you click the widget, the panel will pop up.
  widgets.Widget({
    id: "test-widget",
    label: "Mom",
    contentURL: iconURL,
    panel: myPanel
  });

  // If you run cfx with --static-args='{"quitWhenDone":true}' this program
  // will automatically quit Firefox when it's done.
  if (options.staticArgs.quitWhenDone)
    callbacks.quit();
}
