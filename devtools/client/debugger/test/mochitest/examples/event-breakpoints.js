/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

document.getElementById("click-button").onmousedown = clickHandler;
function clickHandler() {
  document.getElementById("click-target").click();
}

document.getElementById("click-target").onclick = clickTargetClicked;
function clickTargetClicked() {
  console.log("clicked");
}

document.getElementById("xhr-button").onmousedown = xhrHandler;
function xhrHandler() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "doc-event-breakpoints.html", true);
  xhr.onload = function() {
    console.log("xhr load");
  };
  xhr.send();
}

document.getElementById("timer-button").onmousedown = timerHandler;
function timerHandler() {
  setTimeout(() => {
    console.log("timer callback");
  }, 50);
  console.log("timer set");
}
