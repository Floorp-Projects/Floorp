/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global sendAsyncMessage */

/**
 * This script runs in the content process and is attached to browsers when
 * they are created.
 */

// Listen for when the title is changed and send a message back to the chrome
// process.
addEventListener("DOMTitleChanged", function (_ref) {var target = _ref.target;
  sendAsyncMessage("loop@mozilla.org:DOMTitleChanged", { 
    details: "titleChanged" }, 
  { 
    target: target });});
