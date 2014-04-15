/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var self = require("sdk/self");
var { Panel } = require("sdk/panel");
var { ToggleButton } = require("sdk/ui");

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
  var myPanel = Panel({
    contentURL: "data:text/html," + helloHTML,
    onHide: handleHide
  });

  // Create a widget that displays the image.  We'll attach the panel to it.
  // When you click the widget, the panel will pop up.
  var button = ToggleButton({
    id: "test-widget",
    label: "Mom",
    icon: './mom.png',
    onChange: handleChange
  });

  // If you run cfx with --static-args='{"quitWhenDone":true}' this program
  // will automatically quit Firefox when it's done.
  if (options.staticArgs.quitWhenDone)
    callbacks.quit();
}

function handleChange(state) {
  if (state.checked) {
    myPanel.show({ position: button });
  }
}

function handleHide() {
  button.state('window', { checked: false });
}
