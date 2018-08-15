/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PageMetadataChild"];

ChromeUtils.import("resource://gre/actors/ActorChild.jsm");

ChromeUtils.defineModuleGetter(this, "ContextMenuChild",
                               "resource:///modules/ContextMenuChild.jsm");
ChromeUtils.defineModuleGetter(this, "PageMetadata",
                               "resource://gre/modules/PageMetadata.jsm");

class PageMetadataChild extends ActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "PageMetadata:GetPageData": {
        let target = ContextMenuChild.getTarget(this.mm, message);
        let result = PageMetadata.getData(this.content.document, target);
        this.mm.sendAsyncMessage("PageMetadata:PageDataResult", result);
        break;
      }
      case "PageMetadata:GetMicroformats": {
        let target = ContextMenuChild.getTarget(this.mm, message);
        let result = PageMetadata.getMicroformats(this.content.document, target);
        this.mm.sendAsyncMessage("PageMetadata:MicroformatsResult", result);
        break;
      }
    }
  }
}
