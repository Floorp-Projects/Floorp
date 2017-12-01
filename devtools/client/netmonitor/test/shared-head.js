/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported EVENTS, waitForExistingRequests */

"use strict";

const { EVENTS } = require("devtools/client/netmonitor/src/constants");

async function waitForExistingRequests(monitor) {
  let { store } = monitor.panelWin;
  function getRequests() {
    return store.getState().requests.requests;
  }
  function areAllRequestsFullyLoaded() {
    let requests = getRequests().valueSeq();
    for (let request of requests) {
      // Ignore cloned request as we don't lazily fetch data for them
      // and have arbitrary number of field set.
      if (request.id.includes("-clone")) {
        continue;
      }
      // Do same check than FirefoxDataProvider.isRequestPayloadReady,
      // in order to ensure there is no more pending payload requests to be done.
      if (!request.requestHeaders || !request.requestCookies ||
          !request.eventTimings ||
          ((!request.responseHeaders || !request.responseCookies) &&
            request.securityState != "broken" &&
            (!request.responseContentAvailable || request.status))) {
        return false;
      }
    }
    return true;
  }
  // If there is no request, we are good to go.
  if (getRequests().size == 0) {
    return;
  }
  while (!areAllRequestsFullyLoaded()) {
    await monitor.panelWin.once(EVENTS.PAYLOAD_READY);
  }
}
