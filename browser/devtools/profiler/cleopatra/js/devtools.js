/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gInstanceUID;
var gParsedQS;
var gHideSourceLinks;

function getParam(key) {
  if (gParsedQS)
    return gParsedQS[key];

  var query = window.location.search.substring(1);
  gParsedQS = {};

  query.split("&").forEach(function (pair) {
    pair = pair.split("=");
    gParsedQS[decodeURIComponent(pair[0])] = decodeURIComponent(pair[1]);
  });

  return gParsedQS[key];
}

/**
 * Sends a message to the parent window with a status
 * update.
 *
 * @param string status
 *   Status to send to the parent page:
 *    - loaded, when page is loaded.
 *    - displaysource, when user wants to display source
 * @param object data (optional)
 *    Additional data to send to the parent page.
 */
function notifyParent(status, data={}) {
  if (!gInstanceUID) {
    gInstanceUID = getParam("uid");
  }

  window.parent.postMessage({
    uid: gInstanceUID,
    status: status,
    data: data
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
  var profilerMessage = document.getElementById("profilerMessage");
  var msg = JSON.parse(event.data);

  if (msg.task !== "receiveProfileData" && !msg.isCurrent) {
    return;
  }

  switch (msg.task) {
    case "onStarted":
      start.style.display = "none";
      start.querySelector("button").removeAttribute("disabled");
      stop.style.display = "inline";
      break;
    case "onStopped":
      stop.style.display = "none";
      stop.querySelector("button").removeAttribute("disabled");
      start.style.display = "inline";
      break;
    case "receiveProfileData":
      loadProfile(JSON.stringify(msg.rawProfile));
  }
}

window.addEventListener("message", onParentMessage);

/**
 * Main entry point. This function initializes Cleopatra
 * in the light mode and creates all the UI we need.
 */
function initUI() {
  gHideSourceLinks = getParam("ext") === "true";
  gLightMode = true;

  gFileList = { profileParsingFinished: function () {} };
  gInfoBar = { display: function () {} };

  var container = document.createElement("div");
  container.id = "ui";

  gMainArea = document.createElement("div");
  gMainArea.id = "mainarea";

  container.appendChild(gMainArea);
  document.body.appendChild(container);
}

/**
 * Modified copy of Cleopatra's enterFinishedProfileUI.
 * By overriding the function we don't need to modify ui.js which helps
 * with updating from upstream.
 */
function enterFinishedProfileUI() {
  var cover = document.createElement("div");
  cover.className = "finishedProfilePaneBackgroundCover";

  var pane = document.createElement("table");
  var rowIndex = 0;
  var currRow;

  pane.style.width = "100%";
  pane.style.height = "100%";
  pane.border = "0";
  pane.cellPadding = "0";
  pane.cellSpacing = "0";
  pane.borderCollapse = "collapse";
  pane.className = "finishedProfilePane";

  gBreadcrumbTrail = new BreadcrumbTrail();
  currRow = pane.insertRow(rowIndex++);
  currRow.insertCell(0).appendChild(gBreadcrumbTrail.getContainer());

  gHistogramView = new HistogramView();
  currRow = pane.insertRow(rowIndex++);
  currRow.insertCell(0).appendChild(gHistogramView.getContainer());

  if (gMeta && gMeta.videoCapture) {
    gVideoPane = new VideoPane(gMeta.videoCapture);
    gVideoPane.onTimeChange(videoPaneTimeChange);
    currRow = pane.insertRow(rowIndex++);
    currRow.insertCell(0).appendChild(gVideoPane.getContainer());
  }

  var tree = document.createElement("div");
  tree.className = "treeContainer";
  tree.style.width = "100%";
  tree.style.height = "100%";

  gTreeManager = new ProfileTreeManager();
  gTreeManager.treeView.setColumns([
    { name: "sampleCount", title: gStrings["Running Time"] },
    { name: "selfSampleCount", title: gStrings["Self"] },
    { name: "resource", title: "" }
  ]);

  currRow = pane.insertRow(rowIndex++);
  currRow.style.height = "100%";

  var cell = currRow.insertCell(0);
  cell.appendChild(tree);
  tree.appendChild(gTreeManager.getContainer());

  gPluginView = new PluginView();
  tree.appendChild(gPluginView.getContainer());

  gMainArea.appendChild(cover);
  gMainArea.appendChild(pane);

  var currentBreadcrumb = gSampleFilters;
  gBreadcrumbTrail.add({
    title: gStrings["Complete Profile"],
    enterCallback: function () {
      gSampleFilters = [];
      filtersChanged();
    }
  });

  if (currentBreadcrumb == null || currentBreadcrumb.length == 0) {
    gTreeManager.restoreSerializedSelectionSnapshot(gRestoreSelection);
    viewOptionsChanged();
  }

  for (var i = 0; i < currentBreadcrumb.length; i++) {
    var filter = currentBreadcrumb[i];
    var forceSelection = null;
    if (gRestoreSelection != null && i == currentBreadcrumb.length - 1) {
      forceSelection = gRestoreSelection;
    }
    switch (filter.type) {
      case "FocusedFrameSampleFilter":
        focusOnSymbol(filter.name, filter.symbolName);
        gBreadcrumbTrail.enterLastItem(forceSelection);
      case "FocusedCallstackPrefixSampleFilter":
        focusOnCallstack(filter.focusedCallstack, filter.name, false);
        gBreadcrumbTrail.enterLastItem(forceSelection);
      case "FocusedCallstackPostfixSampleFilter":
        focusOnCallstack(filter.focusedCallstack, filter.name, true);
        gBreadcrumbTrail.enterLastItem(forceSelection);
      case "RangeSampleFilter":
        gHistogramView.selectRange(filter.start, filter.end);
        gBreadcrumbTrail.enterLastItem(forceSelection);
    }
  }

  // Show platform data?
  if (getParam("spd") !== "true")
    toggleJavascriptOnly();
}

function enterProgressUI() {
  var pane = document.createElement("div");
  var label = document.createElement("a");
  var bar = document.createElement("progress");
  var string = gStrings.getStr("profiler.loading");

  pane.className = "profileProgressPane";
  pane.appendChild(label);
  pane.appendChild(bar);

  var reporter = new ProgressReporter();
  reporter.addListener(function (rep) {
    var progress = rep.getProgress();

    if (label.textContent !== string) {
      label.textContent = string;
    }

    if (isNaN(progress)) {
      bar.removeAttribute("value");
    } else {
      bar.value = progress;
    }
  });

  gMainArea.appendChild(pane);
  Parser.updateLogSetting();

  return reporter;
}
