/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");

function install(aData, aReason) {}

function uninstall(aData, aReason) {}

function startup(aData, reason) {
  Services.mm.loadFrameScript("resource://onboarding/onboarding.js", true);
}

function shutdown(aData, reason) {}
