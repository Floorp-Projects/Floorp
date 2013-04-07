/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
let { Promise } = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {});
let { TargetFactory } = Cu.import("resource:///modules/devtools/Target.jsm", {});
let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});

const EXAMPLE_URL = "http://example.com/browser/browser/devtools/netmonitor/test/";

const SIMPLE_URL = EXAMPLE_URL + "html_simple-test-page.html";
const NAVIGATE_URL = EXAMPLE_URL + "html_navigate-test-page.html";
const CONTENT_TYPE_URL = EXAMPLE_URL + "html_content-type-test-page.html";
const STATUS_CODES_URL = EXAMPLE_URL + "html_status-codes-test-page.html";
const POST_DATA_URL = EXAMPLE_URL + "html_post-data-test-page.html";

const SIMPLE_SJS = EXAMPLE_URL + "sjs_simple-test-server.sjs";
const CONTENT_TYPE_SJS = EXAMPLE_URL + "sjs_content-type-test-server.sjs";
const STATUS_CODES_SJS = EXAMPLE_URL + "sjs_status-codes-test-server.sjs";

const TEST_IMAGE = EXAMPLE_URL + "test-image.png";

// All tests are asynchronous.
waitForExplicitFinish();

// Enable logging for all the relevant tests.
let gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", true);

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
});

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  let deferred = Promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetWindow.focus();
  let tab = targetBrowser.selectedTab = targetBrowser.addTab(aUrl);

  tab.addEventListener("load", function onLoad() {
    tab.removeEventListener("load", onLoad, true);
    deferred.resolve(tab);
  }, true);

  return deferred.promise;
}

function removeTab(aTab, aWindow) {
  info("Removing tab.");

  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetBrowser.removeTab(aTab);
}

function initNetMonitor(aUrl, aWindow) {
  info("Initializing a network monitor pane.");

  return addTab(aUrl).then((aTab) => {
    info("Net tab added successfully: " + aUrl);

    let deferred = Promise.defer();
    let debuggee = aTab.linkedBrowser.contentWindow.wrappedJSObject;
    let target = TargetFactory.forTab(aTab);

    gDevTools.showToolbox(target, "netmonitor").then((aToolbox) => {
      info("Netork monitor pane shown successfully.");

      let monitor = aToolbox.getCurrentPanel();
      deferred.resolve([aTab, debuggee, monitor]);
    });

    return deferred.promise;
  });
}

function restartNetMonitor(aMonitor, aNewUrl) {
  info("Restarting the specified network monitor.");

  let deferred = Promise.defer();
  let tab = aMonitor.target.tab;
  let url = aNewUrl || tab.linkedBrowser.contentWindow.wrappedJSObject.location.href;

  aMonitor.once("destroyed", () => initNetMonitor(url).then(deferred.resolve));
  removeTab(tab);

  return deferred.promise;
}

function teardown(aMonitor) {
  info("Destroying the specified network monitor.");

  let deferred = Promise.defer();
  let tab = aMonitor.target.tab;

  aMonitor.once("destroyed", deferred.resolve);
  removeTab(tab);

  return deferred.promise;
}

function waitForNetworkEvents(aMonitor, aGetRequests, aPostRequests = 0) {
  let deferred = Promise.defer();

  let panel = aMonitor.panelWin;
  let genericEvents = 0;
  let postEvents = 0;

  function onGenericEvent() {
    genericEvents++;
    maybeResolve();
  }

  function onPostEvent() {
    postEvents++;
    maybeResolve();
  }

  function maybeResolve() {
    info("> Network events progress: " +
      genericEvents + "/" + ((aGetRequests + aPostRequests) * 13) + ", " +
      postEvents + "/" + (aPostRequests * 2));

    // There are 15 updates which need to be fired for a request to be
    // considered finished. RequestPostData isn't fired for non-POST requests.
    if (genericEvents == (aGetRequests + aPostRequests) * 13 &&
        postEvents == aPostRequests * 2) {

      panel.off("NetMonitor:NetworkEventUpdating:RequestHeaders", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdated:RequestHeaders", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdating:RequestCookies", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdating:RequestPostData", onPostEvent);
      panel.off("NetMonitor:NetworkEventUpdated:RequestPostData", onPostEvent);
      panel.off("NetMonitor:NetworkEventUpdated:RequestCookies", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdating:ResponseHeaders", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdated:ResponseHeaders", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdating:ResponseCookies", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdated:ResponseCookies", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdating:ResponseStart", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdating:ResponseContent", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdated:ResponseContent", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdating:EventTimings", onGenericEvent);
      panel.off("NetMonitor:NetworkEventUpdated:EventTimings", onGenericEvent);

      executeSoon(deferred.resolve);
    }
  }

  panel.on("NetMonitor:NetworkEventUpdating:RequestHeaders", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdated:RequestHeaders", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdating:RequestCookies", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdating:RequestPostData", onPostEvent);
  panel.on("NetMonitor:NetworkEventUpdated:RequestPostData", onPostEvent);
  panel.on("NetMonitor:NetworkEventUpdated:RequestCookies", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdating:ResponseHeaders", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdated:ResponseHeaders", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdating:ResponseCookies", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdated:ResponseCookies", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdating:ResponseStart", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdating:ResponseContent", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdated:ResponseContent", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdating:EventTimings", onGenericEvent);
  panel.on("NetMonitor:NetworkEventUpdated:EventTimings", onGenericEvent);

  return deferred.promise;
}

function verifyRequestItemTarget(aRequestItem, aMethod, aUrl, aData = {}) {
  info("> Verifying: " + aMethod + " " + aUrl + " " + aData.toSource());
  info("> Request: " + aRequestItem.attachment.toSource());

  let { status, type, size, time } = aData;
  let { attachment, target } = aRequestItem

  let uri = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
  let name = uri.fileName || "/";
  let query = uri.query;
  let hostPort = uri.hostPort;

  is(attachment.method, aMethod,
    "The attached method is incorrect.");

  is(attachment.url, aUrl,
    "The attached url is incorrect.");

  is(target.querySelector(".requests-menu-method").getAttribute("value"),
    aMethod, "The displayed method is incorrect.");

  is(target.querySelector(".requests-menu-file").getAttribute("value"),
    name + (query ? "?" + query : ""), "The displayed file is incorrect.");

  is(target.querySelector(".requests-menu-domain").getAttribute("value"),
    hostPort, "The displayed domain is incorrect.");

  if (status !== undefined) {
    let value = target.querySelector(".requests-menu-status").getAttribute("code");
    info("Displayed status: " + value);
    is(value, status, "The displayed status is incorrect.");
  }
  if (type !== undefined) {
    let value = target.querySelector(".requests-menu-type").getAttribute("value");
    info("Displayed type: " + value);
    is(value, type, "The displayed type is incorrect.");
  }
  if (size !== undefined) {
    let value = target.querySelector(".requests-menu-size").getAttribute("value");
    info("Displayed size: " + value);
    is(value, size, "The displayed size is incorrect.");
  }
  if (time !== undefined) {
    let value = target.querySelector(".requests-menu-timings-total").getAttribute("value");
    info("Displayed time: " + value);
    ok(~~(value.match(/[0-9]+/)) >= 0, "The displayed time is incorrect.");
  }
}
