/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu }  = Components;
Cu.import("resource://gre/modules/Services.jsm");

function debug(msg) {
  //dump("BrowserElementChild - " + msg + "\n");
}

// NB: this must happen before we process any messages from
// mozbrowser API clients.
docShell.isActive = true;

function parentDocShell(docshell) {
  if (!docshell) {
    return null;
  }
  let treeitem = docshell.QueryInterface(Ci.nsIDocShellTreeItem);
  return treeitem.parent ? treeitem.parent.QueryInterface(Ci.nsIDocShell) : null;
}

function isTopBrowserElement(docShell) {
  while (docShell) {
    docShell = parentDocShell(docShell);
    if (docShell && docShell.isMozBrowserOrApp) {
      return false;
    }
  }
  return true;
}

var BrowserElementIsReady;

debug(`Might load BE scripts: BEIR: ${BrowserElementIsReady}`);
if (!BrowserElementIsReady) {
  debug("Loading BE scripts")
  if (!("BrowserElementIsPreloaded" in this)) {
    if (isTopBrowserElement(docShell)) {
      if (Services.prefs.getBoolPref("dom.mozInputMethod.enabled")) {
        try {
          Services.scriptloader.loadSubScript("chrome://global/content/forms.js");
        } catch (e) {
        }
      }
    }

    if(Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      // general content apps
      if (isTopBrowserElement(docShell)) {
        Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementCopyPaste.js");
      }
    } else {
      // rocketbar in system app and other in-process case (ex. B2G desktop client)
      Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementCopyPaste.js");
    }

    Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementChildPreload.js");
  }

  function onDestroy() {
    removeMessageListener("browser-element-api:destroy", onDestroy);

    if (api) {
      api.destroy();
    }
    if ("CopyPasteAssistent" in this) {
      CopyPasteAssistent.destroy();
    }

    BrowserElementIsReady = false;
  }
  addMessageListener("browser-element-api:destroy", onDestroy);

  BrowserElementIsReady = true;
} else {
  debug("BE already loaded, abort");
}

sendAsyncMessage('browser-element-api:call', { 'msg_name': 'hello' });
