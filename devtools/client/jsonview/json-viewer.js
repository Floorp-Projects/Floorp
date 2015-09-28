/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

define(function(require, exports, module) {

// ReactJS
const React = require("react");

// RDP Inspector
const { createFactories } = require("./components/reps/rep-utils");
const { MainTabbedArea } = createFactories(require("./components/main-tabbed-area"));

const json = document.getElementById("json");
const headers = document.getElementById("headers");

var jsonData;

try {
  jsonData = JSON.parse(json.textContent);
} catch (err) {
  jsonData = err + "";
}

// Application state object.
var input = {
  jsonText: json.textContent,
  jsonPretty : null,
  json: jsonData,
  headers: JSON.parse(headers.textContent),
  tabActive: 1,
  prettified: false
}

json.remove();
headers.remove();

/**
 * Application actions/commands. This list implements all commands
 * available for the JSON viewer.
 */
input.actions = {
  onCopyJson: function() {
    var value = input.prettified ? input.jsonPretty : input.jsonText;
    postChromeMessage("copy", value);
  },

  onSaveJson: function() {
    var value = input.prettified ? input.jsonPretty : input.jsonText;
    postChromeMessage("save", value);
  },

  onCopyHeaders: function() {
    postChromeMessage("copy-headers", input.headers);
  },

  onSearch: function(value) {
    theApp.setState({searchFilter: value});
  },

  onPrettify: function(data) {
    if (input.prettified) {
      theApp.setState({jsonText: input.jsonText});
    } else {
      if (!input.jsonPretty) {
        input.jsonPretty = JSON.stringify(jsonData, null, "  ");
      }
      theApp.setState({jsonText: input.jsonPretty});
    }

    input.prettified = !input.prettified;
  },
}

/**
 * Render the main application component. It's the main tab bar displayed
 * at the top of the window. This component also represents ReacJS root.
 */
var content = document.getElementById("content");
var theApp = React.render(MainTabbedArea(input), content);

var onResize = event => {
  window.document.body.style.height = window.innerHeight + "px";
  window.document.body.style.width = window.innerWidth + "px";
}

window.addEventListener("resize", onResize);
onResize();

// Send notification event to the window. Can be useful for
// tests as well as extensions.
var event = new CustomEvent("JSONViewInitialized", {});
window.jsonViewInitialized = true;
window.dispatchEvent(event);

// End of json-viewer.js
});

