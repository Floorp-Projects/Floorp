/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gSSBBrowser = null;

function init() {
  gSSBBrowser = document.createXULElement("browser");
  gSSBBrowser.setAttribute("id", "browser");
  gSSBBrowser.setAttribute("type", "content");
  gSSBBrowser.setAttribute("remote", "true");
  document.getElementById("browser-container").appendChild(gSSBBrowser);
  gSSBBrowser.src = window.arguments[0];
}

window.addEventListener("load", init, true);
