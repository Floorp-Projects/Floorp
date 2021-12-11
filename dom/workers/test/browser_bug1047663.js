/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXAMPLE_URL = "https://example.com/browser/dom/workers/test/";
const TAB_URL = EXAMPLE_URL + "bug1047663_tab.html";
const WORKER_URL = EXAMPLE_URL + "bug1047663_worker.sjs";

function test() {
  waitForExplicitFinish();

  (async function() {
    // Disable rcwn to make cache behavior deterministic.
    await SpecialPowers.pushPrefEnv({
      set: [["network.http.rcwn.enabled", false]],
    });

    let tab = await addTab(TAB_URL);

    // Create a worker. Post a message to it, and check the reply. Since the
    // server side JavaScript file returns the first source for the first
    // request, the reply should be "one". If the reply is correct, terminate
    // the worker.
    await createWorkerInTab(tab, WORKER_URL);
    let message = await postMessageToWorkerInTab(tab, WORKER_URL, "ping");
    is(message, "one");
    await terminateWorkerInTab(tab, WORKER_URL);

    // Create a second worker with the same URL. Post a message to it, and check
    // the reply. The server side JavaScript file returns the second source for
    // all subsequent requests, but since the cache is still enabled, the reply
    // should still be "one". If the reply is correct, terminate the worker.
    await createWorkerInTab(tab, WORKER_URL);
    message = await postMessageToWorkerInTab(tab, WORKER_URL, "ping");
    is(message, "one");
    await terminateWorkerInTab(tab, WORKER_URL);

    // Disable the cache in this tab. This should also disable the cache for all
    // workers in this tab.
    await disableCacheInTab(tab);

    // Create a third worker with the same URL. Post a message to it, and check
    // the reply. Since the server side JavaScript file returns the second
    // source for all subsequent requests, and the cache is now disabled, the
    // reply should now be "two". If the reply is correct, terminate the worker.
    await createWorkerInTab(tab, WORKER_URL);
    message = await postMessageToWorkerInTab(tab, WORKER_URL, "ping");
    is(message, "two");
    await terminateWorkerInTab(tab, WORKER_URL);

    removeTab(tab);

    finish();
  })();
}
