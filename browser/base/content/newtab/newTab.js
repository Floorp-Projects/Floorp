/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PageThumbs.jsm");
Cu.import("resource://gre/modules/BackgroundPageThumbs.jsm");
Cu.import("resource://gre/modules/DirectoryLinksProvider.jsm");
Cu.import("resource://gre/modules/NewTabUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Rect",
  "resource://gre/modules/Geometry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
  "resource://gre/modules/UpdateChannel.jsm");

let {
  links: gLinks,
  allPages: gAllPages,
  linkChecker: gLinkChecker,
  pinnedLinks: gPinnedLinks,
  blockedLinks: gBlockedLinks,
  gridPrefs: gGridPrefs
} = NewTabUtils;

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.
    createBundle("chrome://browser/locale/newTab.properties");
});

function newTabString(name) gStringBundle.GetStringFromName('newtab.' + name);

function inPrivateBrowsingMode() {
  return PrivateBrowsingUtils.isWindowPrivate(window);
}

const HTML_NAMESPACE = "http://www.w3.org/1999/xhtml";
const XUL_NAMESPACE = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

#include transformations.js
#include page.js
#include grid.js
#include cells.js
#include sites.js
#include drag.js
#include dragDataHelper.js
#include drop.js
#include dropTargetShim.js
#include dropPreview.js
#include updater.js
#include undo.js
#include search.js

// Everything is loaded. Initialize the New Tab Page.
gPage.init();
