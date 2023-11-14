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
  xhr.onload = function () {
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

document.getElementById("eval-button").onmousedown = evalHandler;
function evalHandler() {
  eval(`
    console.log("eval ran");
    //# sourceURL=https://example.com/eval-test.js
  `);
}

document.getElementById("focus-text").addEventListener("focusin", inputFocused);
function inputFocused() {
  console.log("focused");
}

document.getElementById("focus-text").addEventListener("focusout", inputFocusOut);
function inputFocusOut() {
  console.log("focus lost");
}

document.getElementById("focus-text").addEventListener("compositionstart", inputCompositionStart);
function inputCompositionStart() {
  console.log("composition start");
}

document.getElementById("focus-text").addEventListener("compositionupdate", inputCompositionUpdate);
function inputCompositionUpdate() {
  console.log("composition update");
}

document.getElementById("focus-text").addEventListener("compositionend", inputCompositionEnd);
function inputCompositionEnd() {
  console.log("composition end");
}

document.addEventListener("scrollend", onScrollEnd);
function onScrollEnd() {
  console.log("scroll end");
}

document.getElementById("invokee").addEventListener("invoke", onInvoke);
function onInvoke(event) {
  console.log(event);
}
