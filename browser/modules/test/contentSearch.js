/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_MSG = "ContentSearchTest";
const SERVICE_EVENT_TYPE = "ContentSearchService";
const CLIENT_EVENT_TYPE = "ContentSearchClient";

// Forward events from the in-content service to the test.
content.addEventListener(SERVICE_EVENT_TYPE, event => {
  // The event dispatch code in content.js clones the event detail into the
  // content scope. That's generally the right thing, but causes us to end
  // up with an XrayWrapper to it here, which will screw us up when trying to
  // serialize the object in sendAsyncMessage. Waive Xrays for the benefit of
  // the test machinery.
  sendAsyncMessage(TEST_MSG, Components.utils.waiveXrays(event.detail));
});

// Forward messages from the test to the in-content service.
addMessageListener(TEST_MSG, msg => {
  content.dispatchEvent(
    new content.CustomEvent(CLIENT_EVENT_TYPE, {
      detail: msg.data,
    })
  );

  // If the message is a search, stop the page from loading and then tell the
  // test that it loaded.
  if (msg.data.type == "Search") {
    waitForLoadAndStopIt(msg.data.expectedURL, url => {
      sendAsyncMessage(TEST_MSG, {
        type: "loadStopped",
        url,
      });
    });
  }
});

function waitForLoadAndStopIt(expectedURL, callback) {
  let Ci = Components.interfaces;
  let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebProgress);
  let listener = {
    onStateChange(webProg, req, flags, status) {
      if (req instanceof Ci.nsIChannel) {
        let url = req.originalURI.spec;
        dump("waitForLoadAndStopIt: onStateChange " + url + "\n");
        let docStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                       Ci.nsIWebProgressListener.STATE_START;
        if ((flags & docStart) && webProg.isTopLevel && url == expectedURL) {
          webProgress.removeProgressListener(listener);
          req.cancel(Components.results.NS_ERROR_FAILURE);
          callback(url);
        }
      }
    },
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIWebProgressListener,
      Ci.nsISupportsWeakReference,
    ]),
  };
  webProgress.addProgressListener(listener, Ci.nsIWebProgress.NOTIFY_ALL);
  dump("waitForLoadAndStopIt: Waiting for URL to load: " + expectedURL + "\n");
}
