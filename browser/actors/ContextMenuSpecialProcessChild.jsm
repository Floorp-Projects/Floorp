/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ContextMenuSpecialProcessChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);

/**
 * This module is a workaround for bug 1555154, where the contextmenu event doesn't
 * cause the JS Window Actor Child to be constructed automatically in the parent
 * process or extension process documents.
 */
class ContextMenuSpecialProcessChild extends ActorChild {
  handleEvent(event) {
    if (
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT ||
      Services.appinfo.remoteType == E10SUtils.EXTENSION_REMOTE_TYPE
    ) {
      this.content
        .getWindowGlobalChild()
        .getActor("ContextMenu")
        .handleEvent(event);
    }
  }
}
