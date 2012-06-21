/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/PageThumbs.jsm");
Cu.import("resource:///modules/NewTabUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Rect",
  "resource://gre/modules/Geometry.jsm");

let {
  links: gLinks,
  allPages: gAllPages,
  pinnedLinks: gPinnedLinks,
  blockedLinks: gBlockedLinks
} = NewTabUtils;

let chromeWin = window.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIWebNavigation)
                      .QueryInterface(Ci.nsIDocShellTreeItem)
                      .rootTreeItem
                      .QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindow)
                      .wrappedJSObject;

let inPrivateBrowsingMode = false;

if ("gPrivateBrowsingUI" in chromeWin)
  inPrivateBrowsingMode = chromeWin.gPrivateBrowsingUI.privateWindow;

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.
    createBundle("chrome://browser/locale/newTab.properties");
});

function newTabString(name) gStringBundle.GetStringFromName('newtab.' + name);

const HTML_NAMESPACE = "http://www.w3.org/1999/xhtml";

#include batch.js
#include transformations.js
#include page.js
#include grid.js
#include cells.js
#include sites.js
#include drag.js
#include drop.js
#include dropTargetShim.js
#include dropPreview.js
#include updater.js

// Everything is loaded. Initialize the New Tab Page.
gPage.init();
