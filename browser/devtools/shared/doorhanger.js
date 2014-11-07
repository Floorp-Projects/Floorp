/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc } = require("chrome");
const { Services } = require("resource://gre/modules/Services.jsm");
const { DOMHelpers } = require("resource:///modules/devtools/DOMHelpers.jsm");
const { Task } = require("resource://gre/modules/Task.jsm");
const { Promise } = require("resource://gre/modules/Promise.jsm");
const { setTimeout } = require("sdk/timers");
const { getMostRecentBrowserWindow } = require("sdk/window/utils");

const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const DEV_EDITION_PROMO_URL = "chrome://browser/content/devtools/framework/dev-edition-promo.xul";
const DEV_EDITION_PROMO_ENABLED_PREF = "devtools.devedition.promo.enabled";
const DEV_EDITION_PROMO_SHOWN_PREF = "devtools.devedition.promo.shown";
const DEV_EDITION_PROMO_URL_PREF = "devtools.devedition.promo.url";
const LOCALE = Cc["@mozilla.org/chrome/chrome-registry;1"]
               .getService(Ci.nsIXULChromeRegistry)
               .getSelectedLocale("global");

/**
 * Only show Dev Edition promo if it's enabled (beta channel),
 * if it has not been shown before, and it's a locale build
 * for `en-US`
 */
function shouldDevEditionPromoShow () {
  return Services.prefs.getBoolPref(DEV_EDITION_PROMO_ENABLED_PREF) &&
         !Services.prefs.getBoolPref(DEV_EDITION_PROMO_SHOWN_PREF) &&
         LOCALE === "en-US";
}

let TYPES = {
  // The Developer Edition promo doorhanger, called by
  // opening the toolbox, browser console, WebIDE, or responsive design mode
  // in Beta releases. Only displayed once per profile.
  deveditionpromo: {
    predicate: shouldDevEditionPromoShow,
    success: () => Services.prefs.setBoolPref(DEV_EDITION_PROMO_SHOWN_PREF, true),
    action: () => {
      let url = Services.prefs.getCharPref(DEV_EDITION_PROMO_URL_PREF);
      getGBrowser().selectedTab = getGBrowser().addTab(url);
    },
    url: DEV_EDITION_PROMO_URL
  }
};

let panelAttrs = {
  orient: "vertical",
  hidden: "false",
  consumeoutsideclicks: "true",
  noautofocus: "true",
  align: "start",
  role: "alert"
};

/**
 * Helper to call a doorhanger, defined in `TYPES`, with defined conditions,
 * success handlers and loads its own XUL in a frame. Takes an object with
 * several properties:
 *
 * @param {XULWindow} window
 *        The window that should house the doorhanger.
 * @param {String} type
 *        The type of doorhanger to be displayed is, using the `TYPES` definition.
 * @param {String} selector
 *        The selector that the doorhanger should be appended to within `window`.
 *        Defaults to a XUL Document's `window` element.
 */
exports.showDoorhanger = Task.async(function *({ window, type, anchor }) {
  let { predicate, success, url, action } = TYPES[type];
  // Abort if predicate fails
  if (!predicate()) {
    return;
  }

  // Call success function to set preferences/cleanup immediately,
  // so if triggered multiple times, only happens once (Windows/Linux)
  success();

  // Wait 200ms to prevent flickering where the popup is displayed
  // before the underlying window (Windows 7, 64bit)
  yield wait(200);

  let document = window.document;

  let panel = document.createElementNS(XULNS, "panel");
  let frame = document.createElementNS(XULNS, "iframe");
  let parentEl = document.querySelector("window");

  frame.setAttribute("src", url);
  let close = () => parentEl.removeChild(panel);

  setDoorhangerStyle(panel, frame);

  panel.appendChild(frame);
  parentEl.appendChild(panel);

  yield onFrameLoad(frame);

  panel.openPopup(anchor);

  let closeBtn = frame.contentDocument.querySelector("#close");
  if (closeBtn) {
    closeBtn.addEventListener("click", close);
  }

  let goBtn = frame.contentDocument.querySelector("#go");
  if (goBtn) {
    goBtn.addEventListener("click", () => {
      if (action) {
        action();
      }
      close();
    });
  }
});

function setDoorhangerStyle (panel, frame) {
  Object.keys(panelAttrs).forEach(prop => panel.setAttribute(prop, panelAttrs[prop]));
  panel.style.margin = "20px";
  panel.style.borderRadius = "5px";
  panel.style.border = "none";
  panel.style.MozAppearance = "none";
  panel.style.backgroundColor = "transparent";

  frame.style.borderRadius = "5px";
  frame.setAttribute("flex", "1");
  frame.setAttribute("width", "450");
  frame.setAttribute("height", "179");
}

function onFrameLoad (frame) {
  let { resolve, promise } = Promise.defer();

  if (frame.contentWindow) {
    let domHelper = new DOMHelpers(frame.contentWindow);
    domHelper.onceDOMReady(resolve);
  } else {
    let callback = () => {
      frame.removeEventListener("DOMContentLoaded", callback);
      resolve();
    }
    frame.addEventListener("DOMContentLoaded", callback);
  }

  return promise;
}

function getGBrowser () {
  return getMostRecentBrowserWindow().gBrowser;
}

function wait (n) {
  let { resolve, promise } = Promise.defer();
  setTimeout(resolve, n);
  return promise;
}
