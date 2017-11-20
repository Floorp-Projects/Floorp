/* -*- Mode: javascript; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);

addMessageListener('media-test:getcwd', () => {
  let cwd;
  try {
    cwd = dirSvc.get("CurWorkD", Ci.nsIFile).path;
  } finally {
    sendAsyncMessage('media-test:cwd', cwd);
  }
});
