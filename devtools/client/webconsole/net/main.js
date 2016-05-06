/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://devtools/client/shared/browser-loader.js");

// Initialize module loader and load all modules of the new inline
// preview feature. The entire code-base doesn't need any extra
// privileges and runs entirely in content scope.
const rootUrl = "resource://devtools/client/webconsole/net/";
const require = BrowserLoader({
  baseURI: rootUrl,
  window: this}).require;

const NetRequest = require("./net-request");
const { loadSheet } = require("sdk/stylesheet/utils");

// Localization
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
var networkStrings = Services.strings.createBundle(
  "chrome://devtools/locale/netmonitor.properties");

// Stylesheets
var styleSheets = [
  "resource://devtools/client/jsonview/css/toolbar.css",
  "resource://devtools/client/shared/components/tree/tree-view.css",
  "resource://devtools/client/shared/components/reps/reps.css",
  "resource://devtools/client/webconsole/net/net-request.css",
  "resource://devtools/client/webconsole/net/components/size-limit.css",
  "resource://devtools/client/webconsole/net/components/net-info-body.css",
  "resource://devtools/client/webconsole/net/components/net-info-group.css",
  "resource://devtools/client/webconsole/net/components/net-info-params.css",
  "resource://devtools/client/webconsole/net/components/response-tab.css"
];

// Load theme stylesheets into the Console frame. This should be
// done automatically by UI Components as soon as we have consensus
// on the right CSS strategy FIXME.
// It would also be nice to include them using @import.
styleSheets.forEach(url => {
  loadSheet(this, url, "author");
});

// Localization API used by React components
// accessing strings from *.properties file.
// Example:
//   let localizedString = Locale.$STR('string-key');
//
// Resources:
// http://l20n.org/
// https://github.com/yahoo/react-intl
this.Locale = {
  $STR: key => {
    try {
      return networkStrings.GetStringFromName(key);
    } catch (err) {
      console.error(key + ": " + err);
    }
  }
};

// List of NetRequest instances represents the state.
// As soon as Redux is in place it should be maintained using a reducer.
var netRequests = new Map();

/**
 * This function handles network events received from the backend. It's
 * executed from within the webconsole.js
 */
function onNetworkEvent(log) {
  // The 'from' field is set only in case of a 'networkEventUpdate' packet.
  // The initial 'networkEvent' packet uses 'actor'.
  // Check if NetRequest object is already created for this event actor and
  // if there is none make sure to create one.
  let response = log.response;
  let netRequest = response.from ? netRequests.get(response.from) : null;
  if (!netRequest && !log.update) {
    netRequest = new NetRequest(log);
    netRequests.set(response.actor, netRequest);
  }

  if (!netRequest) {
    return;
  }

  if (log.update) {
    netRequest.updateBody(response);
  }

  return;
}

// Make the 'onNetworkEvent' accessible from chrome (see webconsole.js)
this.NetRequest = {
  onNetworkEvent: onNetworkEvent
};
