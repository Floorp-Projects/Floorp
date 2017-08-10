/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata =  {
  "stability": "experimental"
};

const { Ci } = require("chrome");

const SHEET_TYPE = {
  "agent": "AGENT_SHEET",
  "user": "USER_SHEET",
  "author": "AUTHOR_SHEET"
};

function getDOMWindowUtils(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).
                getInterface(Ci.nsIDOMWindowUtils);
};

/**
 * Synchronously loads a style sheet from `uri` and adds it to the list of
 * additional style sheets of the document.
 * The sheets added takes effect immediately, and only on the document of the
 * `window` given.
 */
function loadSheet(window, url, type) {
  if (!(type && type in SHEET_TYPE))
    type = "author";

  type = SHEET_TYPE[type];

  if (url instanceof Ci.nsIURI)
    url = url.spec;

  let winUtils = getDOMWindowUtils(window);
  try {
    winUtils.loadSheetUsingURIString(url, winUtils[type]);
  }
  catch (e) {};
};
exports.loadSheet = loadSheet;

/**
 * Remove the document style sheet at `sheetURI` from the list of additional
 * style sheets of the document.  The removal takes effect immediately.
 */
function removeSheet(window, url, type) {
  if (!(type && type in SHEET_TYPE))
    type = "author";

  type = SHEET_TYPE[type];

  if (url instanceof Ci.nsIURI)
    url = url.spec;

  let winUtils = getDOMWindowUtils(window);

  try {
    winUtils.removeSheetUsingURIString(url, winUtils[type]);
  }
  catch (e) {};
};
exports.removeSheet = removeSheet;

/**
 * Returns `true` if the `type` given is valid, otherwise `false`.
 * The values currently accepted are: "agent", "user" and "author".
 */
function isTypeValid(type) {
  return type in SHEET_TYPE;
}
exports.isTypeValid = isTypeValid;
