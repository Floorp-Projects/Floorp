/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = [];

const { FileUtils } = ChromeUtils.import(
    "resource://gre/modules/FileUtils.jsm"
);

function OpenLinkInExternal(path, url) {
    const process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
    process.init(path);
    process.runAsync([url], 1) 
}
OpenLinkInExternal(FileUtils.File("C:\\Users\\user\\AppData\\Local\\Vivaldi\\Application\\vivaldi.exe"), "https://google.com")
