/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gInstanceUID;

/**
 * Sends a message to the parent window with a status
 * update.
 *
 * @param string status
 *   Status to send to the parent page:
 *    - loaded, when page is loaded.
 *    - start, when user wants to start profiling.
 *    - stop, when user wants to stop profiling.
 */
function notifyParent(status) {
  if (!gInstanceUID) {
    gInstanceUID = window.location.search.substr(1);
  }

  window.parent.postMessage({
    uid: gInstanceUID,
    status: status
  }, "*");
}

/**
 * A listener for incoming messages from the parent
 * page. All incoming messages must be stringified
 * JSON objects to be compatible with Cleopatra's
 * format:
 *
 * {
 *   task: string,
 *   ...
 * }
 *
 * This listener recognizes two tasks: onStarted and
 * onStopped.
 *
 * @param object event
 *   PostMessage event object.
 */
function onParentMessage(event) {
  var start = document.getElementById("startWrapper");
  var stop = document.getElementById("stopWrapper");
  var msg = JSON.parse(event.data);

  if (msg.task === "onStarted") {
    start.style.display = "none";
    start.querySelector("button").removeAttribute("disabled");
    stop.style.display = "inline";
  } else if (msg.task === "onStopped") {
    stop.style.display = "none";
    stop.querySelector("button").removeAttribute("disabled");
    start.style.display = "inline";
  }
}

window.addEventListener("message", onParentMessage);

/**
 * Main entry point. This function initializes Cleopatra
 * in the light mode and creates all the UI we need.
 */
function initUI() {
  gLightMode = true;
  gJavaScriptOnly = true;

  var container = document.createElement("div");
  container.id = "ui";

  gMainArea = document.createElement("div");
  gMainArea.id = "mainarea";

  container.appendChild(gMainArea);
  document.body.appendChild(container);

  var startButton = document.createElement("button");
  startButton.innerHTML = "Start";
  startButton.addEventListener("click", function (event) {
    event.target.setAttribute("disabled", true);
    notifyParent("start");
  }, false);

  var stopButton = document.createElement("button");
  stopButton.innerHTML = "Stop";
  stopButton.addEventListener("click", function (event) {
    event.target.setAttribute("disabled", true);
    notifyParent("stop");
  }, false);

  var controlPane = document.createElement("div");
  controlPane.className = "controlPane";
  controlPane.innerHTML =
    "<p id='startWrapper'>Click <span class='btn'></span> to start profiling.</p>" +
    "<p id='stopWrapper'>Click <span class='btn'></span> to stop profiling.</p>";

  controlPane.querySelector("#startWrapper > span.btn").appendChild(startButton);
  controlPane.querySelector("#stopWrapper > span.btn").appendChild(stopButton);

  gMainArea.appendChild(controlPane);
}