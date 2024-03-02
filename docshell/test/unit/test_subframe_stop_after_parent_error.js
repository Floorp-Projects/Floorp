"use strict";
// Tests that pending subframe requests for an initial about:blank
// document do not delay showing load errors (and possibly result in a
// crash at docShell destruction) for failed document loads.

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/undefined/);

const { XPCShellContentUtils } = ChromeUtils.importESModule(
  "resource://testing-common/XPCShellContentUtils.sys.mjs"
);

XPCShellContentUtils.init(this);

const server = XPCShellContentUtils.createHttpServer({
  hosts: ["example.com"],
});

// Registers a URL with the HTTP server which will not return a response
// until we're ready to.
function registerSlowPage(path) {
  let result = {
    url: `http://example.com/${path}`,
  };

  let finishedPromise = new Promise(resolve => {
    result.finish = resolve;
  });

  server.registerPathHandler(`/${path}`, async (request, response) => {
    response.processAsync();

    response.setHeader("Content-Type", "text/html");
    response.write("<html><body>Hello.</body></html>");

    await finishedPromise;

    response.finish();
  });

  return result;
}

let topFrameRequest = registerSlowPage("top.html");
let subFrameRequest = registerSlowPage("frame.html");

let thunks = new Set();
function promiseStateStop(webProgress) {
  return new Promise(resolve => {
    let listener = {
      onStateChange(aWebProgress, request, stateFlags) {
        if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
          webProgress.removeProgressListener(listener);

          thunks.delete(listener);
          resolve();
        }
      },

      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };

    // Keep the listener alive, since it's stored as a weak reference.
    thunks.add(listener);
    webProgress.addProgressListener(
      listener,
      Ci.nsIWebProgress.NOTIFY_STATE_NETWORK
    );
  });
}

async function runTest(waitForErrorPage) {
  let page = await XPCShellContentUtils.loadContentPage("about:blank");

  // Watch for the HTTP request for the top frame so that we can cancel
  // it with an error.
  let requestPromise = TestUtils.topicObserved(
    "http-on-modify-request",
    subject => subject.QueryInterface(Ci.nsIRequest).name == topFrameRequest.url
  );

  await page.spawn(
    [topFrameRequest.url, subFrameRequest.url],
    function (topFrameUrl, subFrameRequestUrl) {
      // Create a frame with a source URL which will not return a response
      // before we cancel it with an error.
      let doc = this.content.document;
      let frame = doc.createElement("iframe");
      frame.src = topFrameUrl;
      doc.body.appendChild(frame);

      // Create a subframe in the initial about:blank document for the above
      // frame which will not return a response before we cancel the
      // document request.
      let frameDoc = frame.contentDocument;
      let subframe = frameDoc.createElement("iframe");
      subframe.src = subFrameRequestUrl;
      frameDoc.body.appendChild(subframe);
    }
  );

  let [req] = await requestPromise;

  info("Cancel request for parent frame");
  req.cancel(Cr.NS_ERROR_PROXY_CONNECTION_REFUSED);

  // Request cancelation is not synchronous, so wait for the STATE_STOP
  // event to fire.
  await promiseStateStop(page.browsingContext.webProgress);

  // And make a trip through the event loop to give the DocLoader a
  // chance to update its state.
  await new Promise(executeSoon);

  if (waitForErrorPage) {
    // Make sure that canceling the request with an error code actually
    // shows an error page without waiting for a subframe response.
    await TestUtils.waitForCondition(() =>
      page.browsingContext.children[0]?.currentWindowGlobal?.documentURI?.spec.startsWith(
        "about:neterror?"
      )
    );
  }

  await page.close();
}

add_task(async function testRemoveFrameImmediately() {
  await runTest(false);
});

add_task(async function testRemoveFrameAfterErrorPage() {
  await runTest(true);
});

add_task(async function () {
  // Allow the document requests for the frames to complete.
  topFrameRequest.finish();
  subFrameRequest.finish();
});
