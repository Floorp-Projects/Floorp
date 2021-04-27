/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- /
/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const NS_GFXINFO_CONTRACTID = "@mozilla.org/gfx/info;1";

var gContainingWindow = null;

var gBrowser;

function OnDocumentLoad(evt) {
  if (
    evt.target != gBrowser.contentDocument ||
    evt.target.location == "about:blank"
  ) {
    return;
  }
  gBrowser.removeEventListener("load", OnDocumentLoad, true);
  gContainingWindow.close();
}

this.OnRecordingLoad = function OnRecordingLoad(win) {
  if (win === undefined || win == null) {
    win = window;
  }
  if (gContainingWindow == null && win != null) {
    gContainingWindow = win;
  }

  gBrowser = gContainingWindow.document.getElementById("browser");

  var gfxInfo =
    NS_GFXINFO_CONTRACTID in Cc &&
    Cc[NS_GFXINFO_CONTRACTID].getService(Ci.nsIGfxInfo);
  dump(gfxInfo.AzureContentBackend + "\n");
  if (gfxInfo.AzureContentBackend == "none") {
    alert("Page recordings may only be made with Azure content enabled.");
    gContainingWindow.close();
    return;
  }

  gBrowser.addEventListener("load", OnDocumentLoad, true);

  var args = window.arguments[0].wrappedJSObject;

  gBrowser.loadURI(args.uri, {
    triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
  });
};
