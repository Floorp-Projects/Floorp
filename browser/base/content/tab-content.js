/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script contains code that requires a tab browser. */

/* eslint-env mozilla/frame-script */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// BrowserChildGlobal
var global = this;

Services.obs.notifyObservers(this, "tab-content-frameloader-created");

// This is a temporary hack to prevent regressions (bug 1471327).
void content;
