/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ContextMenuParent"];

class ContextMenuParent extends JSWindowActorParent {
  receiveMessage(message) {
    let browser = this.manager.rootFrameLoader.ownerElement;
    let win = browser.ownerGlobal;
    // It's possible that the <xul:browser> associated with this
    // ContextMenu message doesn't belong to a window that actually
    // loads nsContextMenu.js. In that case, try to find the chromeEventHandler,
    // since that'll likely be the "top" <xul:browser>, and then use its window's
    // nsContextMenu instance instead.
    if (!win.openContextMenu) {
      let topBrowser = browser.ownerGlobal.docShell.chromeEventHandler;
      win = topBrowser.ownerGlobal;
    }

    win.openContextMenu(message, browser, this);
  }

  hiding() {
    try {
      this.sendAsyncMessage("ContextMenu:Hiding", {});
    } catch (e) {
      // This will throw if the content goes away while the
      // context menu is still open.
    }
  }

  reloadFrame(targetIdentifier, forceReload) {
    this.sendAsyncMessage("ContextMenu:ReloadFrame", {
      targetIdentifier,
      forceReload,
    });
  }

  getImageText(targetIdentifier) {
    this.sendAsyncMessage("ContextMenu:GetImageText", {
      targetIdentifier,
    });
  }

  toggleRevealPassword(targetIdentifier) {
    this.sendAsyncMessage("ContextMenu:ToggleRevealPassword", {
      targetIdentifier,
    });
  }

  reloadImage(targetIdentifier) {
    this.sendAsyncMessage("ContextMenu:ReloadImage", { targetIdentifier });
  }

  getFrameTitle(targetIdentifier) {
    return this.sendQuery("ContextMenu:GetFrameTitle", { targetIdentifier });
  }

  mediaCommand(targetIdentifier, command, data) {
    let windowGlobal = this.manager.browsingContext.currentWindowGlobal;
    let browser = windowGlobal.rootFrameLoader.ownerElement;
    let win = browser.ownerGlobal;
    let windowUtils = win.windowUtils;
    this.sendAsyncMessage("ContextMenu:MediaCommand", {
      targetIdentifier,
      command,
      data,
      handlingUserInput: windowUtils.isHandlingUserInput,
    });
  }

  canvasToBlobURL(targetIdentifier) {
    return this.sendQuery("ContextMenu:Canvas:ToBlobURL", { targetIdentifier });
  }

  saveVideoFrameAsImage(targetIdentifier) {
    return this.sendQuery("ContextMenu:SaveVideoFrameAsImage", {
      targetIdentifier,
    });
  }

  setAsDesktopBackground(targetIdentifier) {
    return this.sendQuery("ContextMenu:SetAsDesktopBackground", {
      targetIdentifier,
    });
  }

  getSearchFieldBookmarkData(targetIdentifier) {
    return this.sendQuery("ContextMenu:SearchFieldBookmarkData", {
      targetIdentifier,
    });
  }
}
