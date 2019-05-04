#!/usr/bin/env node

/* eslint-disable no-console */

const {cp, set} = require("shelljs");
const path = require("path");

const filesToVendor = {
  // XXX currently these two licenses are identical.  Perhaps we should check
  // in case that changes at some point in the future.
  "react/LICENSE": "REACT_AND_REACT_DOM_LICENSE",
  "react/umd/react.production.min.js": "react.js",
  "react/umd/react.development.js": "react-dev.js",
  "react-dom/umd/react-dom.production.min.js": "react-dom.js",
  "react-dom/umd/react-dom.development.js": "react-dom-dev.js",
  "react-intl/LICENSE.md": "REACT_INTL_LICENSE",
  "react-intl/dist/react-intl.min.js": "react-intl.js",
  "react-redux/LICENSE.md": "REACT_REDUX_LICENSE",
  "react-redux/dist/react-redux.min.js": "react-redux.js",
};

set("-v"); // Echo all the copy commands so the user can see what's going on
for (let srcPath of Object.keys(filesToVendor)) {
  cp(path.join("node_modules", srcPath),
    path.join("vendor", filesToVendor[srcPath]));
}

console.log(`
Check to see if any license files have changed, and, if so, be sure to update
https://searchfox.org/mozilla-central/source/toolkit/content/license.html`);
