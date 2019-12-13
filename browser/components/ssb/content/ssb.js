/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineModuleGetter(
  this,
  "SiteSpecificBrowser",
  "resource:///modules/SiteSpecificBrowserService.jsm"
);

let gSSBBrowser = null;
let gSSB = null;

function init() {
  gSSB = SiteSpecificBrowser.get(window.arguments[0]);

  gSSBBrowser = document.createXULElement("browser");
  gSSBBrowser.setAttribute("id", "browser");
  gSSBBrowser.setAttribute("type", "content");
  gSSBBrowser.setAttribute("remote", "true");
  document.getElementById("browser-container").appendChild(gSSBBrowser);
  gSSBBrowser.src = gSSB.startURI.spec;
}

window.addEventListener("load", init, true);
