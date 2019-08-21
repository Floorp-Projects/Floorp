#!/usr/bin/env node
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-console */

const { cp, set } = require("shelljs");
const path = require("path");

const filesToVendor = {
  // XXX currently these two licenses are identical.  Perhaps we should check
  // in case that changes at some point in the future.
  "react/LICENSE": "REACT_AND_REACT_DOM_LICENSE",
  "react/umd/react.production.min.js": "react.js",
  "react/umd/react.development.js": "react-dev.js",
  "react-dom/umd/react-dom.production.min.js": "react-dom.js",
  "react-dom/umd/react-dom.development.js": "react-dom-dev.js",
  "react-redux/LICENSE.md": "REACT_REDUX_LICENSE",
  "react-redux/dist/react-redux.min.js": "react-redux.js",
  "react-transition-group/dist/react-transition-group.min.js":
    "react-transition-group.js",
  "react-transition-group/LICENSE": "REACT_TRANSITION_GROUP_LICENSE",
};

set("-v"); // Echo all the copy commands so the user can see what's going on
for (let srcPath of Object.keys(filesToVendor)) {
  cp(
    path.join("node_modules", srcPath),
    path.join("vendor", filesToVendor[srcPath])
  );
}

console.log(`
Check to see if any license files have changed, and, if so, be sure to update
https://searchfox.org/mozilla-central/source/toolkit/content/license.html`);
