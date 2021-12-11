/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ContextMenuChild",
  "resource:///actors/ContextMenuChild.jsm"
);

this.menusChild = class extends ExtensionAPI {
  getAPI(context) {
    return {
      menus: {
        getTargetElement(targetElementId) {
          let element;
          let lastMenuTarget = ContextMenuChild.getLastTarget(
            context.contentWindow.docShell.browsingContext
          );
          if (
            lastMenuTarget &&
            Math.floor(lastMenuTarget.timeStamp) === targetElementId
          ) {
            element = lastMenuTarget.targetRef.get();
          }
          if (
            element &&
            element.getRootNode({ composed: true }) ===
              context.contentWindow.document
          ) {
            return element;
          }
          return null;
        },
      },
    };
  }
};
