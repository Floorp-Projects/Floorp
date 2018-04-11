/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { require } =
  ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const QR = require("devtools/shared/qrcode/index");

window.addEventListener("load", function() {
  document.getElementById("close").onclick = () => window.close();
  document.getElementById("no-scanner").onclick = showToken;
  document.getElementById("yes-scanner").onclick = hideToken;
  buildUI();
}, {once: true});

function buildUI() {
  let { oob } = window.arguments[0];
  createQR(oob);
  createToken(oob);
}

function createQR(oob) {
  let oobData = JSON.stringify(oob);
  let imgData = QR.encodeToDataURI(oobData, "L" /* low quality */);
  document.querySelector("#qr-code img").src = imgData.src;
}

function createToken(oob) {
  let token = oob.sha256.replace(/:/g, "").toLowerCase() + oob.k;
  document.querySelector("#token pre").textContent = token;
}

function showToken() {
  document.querySelector("body").setAttribute("token", "true");
}

function hideToken() {
  document.querySelector("body").removeAttribute("token");
}
