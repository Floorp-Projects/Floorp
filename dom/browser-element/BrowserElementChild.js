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
    if (docShell && docShell.isBrowserOrApp) {
      return false;
    }
  }
  return true;
}

if (!('BrowserElementIsPreloaded' in this)) {
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

  if (Services.prefs.getIntPref("dom.w3c_touch_events.enabled") != 0) {
    if (docShell.asyncPanZoomEnabled === false) {
      Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementPanningAPZDisabled.js");
      ContentPanningAPZDisabled.init();
    }

    Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementPanning.js");
    ContentPanning.init();
  }

  Services.scriptloader.loadSubScript("chrome://global/content/BrowserElementChildPreload.js");
} else {
  if (Services.prefs.getIntPref("dom.w3c_touch_events.enabled") == 1) {
    if (docShell.asyncPanZoomEnabled === false) {
      ContentPanningAPZDisabled.init();
    }
    ContentPanning.init();
  }
}

var BrowserElementIsReady = true;


sendAsyncMessage('browser-element-api:call', { 'msg_name': 'hello' });
