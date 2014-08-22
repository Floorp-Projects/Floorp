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

function newTabString(name, args) {
  switch (name) {
    case "customize.title":
      return "Customize your New Tab page";

    case "customize.enhanced":
      return "Enhanced";

    case "customize.classic":
      return "Classic";

    case "customize.blank":
      return "Blank";

    case "sponsored.button":
      return "SPONSORED";

    case "sponsored.explain":
      return "This tile is being shown to you on behalf of a Mozilla partner. You can remove it at any time by clicking the %1$S button. %2$S".replace("%1$S", args[0]).replace("%2$S", args[1]);

    case "enhanced.explain":
      return "A Mozilla partner has visually enhanced this tile, replacing the screenshot. You can turn off enhanced tiles by clicking the %1$S button for your preferences. %2$S".replace("%1$S", args[0]).replace("%2$S", args[1]);

    case "learn.link":
      return "Learn more…";
  }

  let stringName = "newtab." + name;
  if (!args) {
    return gStringBundle.GetStringFromName(stringName);
  }
  return gStringBundle.formatStringFromName(stringName, args, args.length);
}

function inPrivateBrowsingMode() {
  return PrivateBrowsingUtils.isWindowPrivate(window);
}

const HTML_NAMESPACE = "http://www.w3.org/1999/xhtml";
const XUL_NAMESPACE = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const TILES_EXPLAIN_LINK = "https://support.mozilla.org/kb/how-do-sponsored-tiles-work";

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
#include customize.js

// Everything is loaded. Initialize the New Tab Page.
gPage.init();
