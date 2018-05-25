/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["LightWeightThemeWebInstallListener"];

var LightWeightThemeWebInstallListener = {
  _previewWindow: null,

  handleEvent(event) {
    let mm = getMessageManagerForContent(event.target.ownerGlobal);
    switch (event.type) {
      case "InstallBrowserTheme": {
        mm.sendAsyncMessage("LightWeightThemeWebInstaller:Install", {
          baseURI: event.target.baseURI,
          principal: event.target.nodePrincipal,
          themeData: event.target.getAttribute("data-browsertheme"),
        });
        break;
      }
      case "PreviewBrowserTheme": {
        mm.sendAsyncMessage("LightWeightThemeWebInstaller:Preview", {
          baseURI: event.target.baseURI,
          principal: event.target.nodePrincipal,
          themeData: event.target.getAttribute("data-browsertheme"),
        });
        this._previewWindow = event.target.ownerGlobal;
        this._previewWindow.addEventListener("pagehide", this, true);
        break;
      }
      case "pagehide": {
        mm.sendAsyncMessage("LightWeightThemeWebInstaller:ResetPreview");
        this._resetPreviewWindow();
        break;
      }
      case "ResetBrowserThemePreview": {
        if (this._previewWindow) {
          mm.sendAsyncMessage("LightWeightThemeWebInstaller:ResetPreview",
                           {principal: event.target.nodePrincipal});
          this._resetPreviewWindow();
        }
        break;
      }
    }
  },

  _resetPreviewWindow() {
    this._previewWindow.removeEventListener("pagehide", this, true);
    this._previewWindow = null;
  }
};

function getMessageManagerForContent(content) {
  return content.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDocShell)
                .sameTypeRootTreeItem
                .QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIContentFrameMessageManager);
}
