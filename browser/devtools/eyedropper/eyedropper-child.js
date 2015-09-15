/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { interfaces: Ci } = Components;

addMessageListener("Eyedropper:RequestContentScreenshot", sendContentScreenshot);

function sendContentScreenshot() {
  let canvas = content.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  let scale = content.getInterface(Ci.nsIDOMWindowUtils).fullZoom;
  let width = content.innerWidth;
  let height = content.innerHeight;
  canvas.width = width * scale;
  canvas.height = height * scale;
  canvas.mozOpaque = true;

  let ctx = canvas.getContext("2d");

  ctx.scale(scale, scale);
  ctx.drawWindow(content, content.scrollX, content.scrollY, width, height, "#fff");

  sendAsyncMessage("Eyedropper:Screenshot", canvas.toDataURL());
}
