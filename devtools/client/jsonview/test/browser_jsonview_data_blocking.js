/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const JSON_VIEW_MIME_TYPE = "application/vnd.mozilla.json.view";
const nullP = Services.scriptSecurityManager.createNullPrincipal({});

// We need 3 levels of nesting just to get to run something against a content
// page, so the devtools limit of 4 levels of nesting don't help:
/* eslint max-nested-callbacks: 0 */

/**
 * Check that we don't expose a JSONView object on data: URI windows where
 * we block the load.
 */
add_task(async function test_blocked_data_exposure() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", true]],
  });
  await BrowserTestUtils.withNewTab(TEST_PATH + "empty.html", async browser => {
    const tabCount = gBrowser.tabs.length;
    await ContentTask.spawn(browser, null, function() {
      content.w = content.window.open(
        "data:application/vnd.mozilla.json.view,1",
        "_blank"
      );
      ok(
        !Cu.waiveXrays(content.w).JSONView,
        "Should not have created a JSON View object"
      );
      // We need to wait for the JSON view machinery to actually have a chance to run.
      // We have no way to detect that it has or hasn't, so a setTimeout is the best we
      // can do, unfortunately.
      return new Promise(resolve => {
        content.setTimeout(function() {
          // Putting the resolve before the check to avoid JS errors potentially causing
          // the test to time out.
          resolve();
          ok(
            !Cu.waiveXrays(content.w).JSONView,
            "Should still not have a JSON View object"
          );
        }, 1000);
      });
    });
    // Without this, if somehow the data: protocol blocker stops working, the
    // test would just keep passing.
    is(
      tabCount,
      gBrowser.tabs.length,
      "Haven't actually opened a new window/tab"
    );
  });
});

/**
 * Check that aborted channels also abort sending data from the stream converter.
 */
add_task(async function test_converter_abort_should_stop_data_sending() {
  const loadInfo = NetUtil.newChannel({
    uri: Services.io.newURI("data:text/plain,"),
    loadingPrincipal: nullP,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  }).loadInfo;
  // Stub all the things.
  const chan = {
    QueryInterface: ChromeUtils.generateQI([
      Ci.nsIChannel,
      Ci.nsIWritablePropertyBag,
    ]),
    URI: Services.io.newURI("data:application/json,{}"),
    // loadinfo is builtinclass, need to actually have one:
    loadInfo,
    notificationCallbacks: {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIInterfaceRequestor]),
      getInterface() {
        // We want a loadcontext here, which is also builtinclass, can't stub.
        return docShell;
      },
    },
    status: Cr.NS_OK,
    setProperty() {},
  };
  let onStartFired = false;
  const listener = {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIStreamListener]),
    onStartRequest() {
      onStartFired = true;
      // This should force the converter to abort, too:
      chan.status = Cr.NS_BINDING_ABORTED;
    },
    onDataAvailable() {
      ok(false, "onDataAvailable should never fire");
    },
  };
  const conv = Cc[
    "@mozilla.org/streamconv;1?from=" + JSON_VIEW_MIME_TYPE + "&to=*/*"
  ].createInstance(Ci.nsIStreamConverter);
  conv.asyncConvertData(
    "application/vnd.mozilla.json.view",
    "text/html",
    listener,
    null
  );
  conv.onStartRequest(chan);
  ok(onStartFired, "Should have fired onStartRequest");
});

/**
 * Check that principal mismatches break things. Note that we're stubbing
 * the window associated with the channel to be a browser window; the
 * converter should be bailing out because the window's principal won't
 * match the null principal to which the converter tries to reset the
 * inheriting principal of the channel.
 */
add_task(async function test_converter_principal_needs_matching() {
  const loadInfo = NetUtil.newChannel({
    uri: Services.io.newURI("data:text/plain,"),
    loadingPrincipal: nullP,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  }).loadInfo;
  // Stub all the things.
  const chan = {
    QueryInterface: ChromeUtils.generateQI([
      Ci.nsIChannel,
      Ci.nsIWritablePropertyBag,
    ]),
    URI: Services.io.newURI("data:application/json,{}"),
    // loadinfo is builtinclass, need to actually have one:
    loadInfo,
    notificationCallbacks: {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIInterfaceRequestor]),
      getInterface() {
        // We want a loadcontext here, which is also builtinclass, can't stub.
        return docShell;
      },
    },
    status: Cr.NS_OK,
    setProperty() {},
    cancel(arg) {
      this.status = arg;
    },
  };
  let onStartFired = false;
  const listener = {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIStreamListener]),
    onStartRequest() {
      onStartFired = true;
    },
    onDataAvailable() {
      ok(false, "onDataAvailable should never fire");
    },
  };
  const conv = Cc[
    "@mozilla.org/streamconv;1?from=" + JSON_VIEW_MIME_TYPE + "&to=*/*"
  ].createInstance(Ci.nsIStreamConverter);
  conv.asyncConvertData(
    "application/vnd.mozilla.json.view",
    "text/html",
    listener,
    null
  );
  conv.onStartRequest(chan);
  ok(onStartFired, "Should have fired onStartRequest");
  is(chan.status, Cr.NS_BINDING_ABORTED, "Should have been aborted.");
});
