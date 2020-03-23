/* Any copyright is dedicated to the Public Domain.
 *  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if resending an image request uses the same content type
 * and hence is not blocked by the CSP of the page.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CSP_RESEND_URL, {
    requestCount: 1,
  });
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Executes 1 request
  await performRequests(monitor, tab, 1);

  // Select the image request
  const imgRequest = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, imgRequest);

  // Stores original request for comparison of values later
  const { getSelectedRequest } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  const origReq = getSelectedRequest(store.getState());

  // Context Menu > "Resend"
  EventUtils.sendMouseEvent({ type: "contextmenu" }, imgRequest);
  getContextMenuItem(monitor, "request-list-context-resend-only").click();

  await performRequests(monitor, tab, 1);

  // Selects request that was resent
  const selReq = getSelectedRequest(store.getState());

  // Finally, some sanity checks
  ok(selReq.url.endsWith("test-image.png"), "Correct request selected");
  ok(origReq.url === selReq.url, "Orig and Sel url match");

  ok(selReq.cause.type === "img", "Correct type of selected");
  ok(origReq.cause.type === selReq.cause.type, "Orig and Sel type match");

  const cspOBJ = await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    return JSON.parse(content.document.cspJSON);
  });

  const policies = cspOBJ["csp-policies"];
  is(policies.length, 1, "CSP: should be one policy");
  const policy = policies[0];
  is(policy["img-src"], "*", "CSP: img-src should be *");

  return teardown(monitor);
});
