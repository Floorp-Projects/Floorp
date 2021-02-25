/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

// Import helpers for the workers
/* import-globals-from helper_workers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_workers.js",
  this
);

var TAB_URL = EXAMPLE_URL + "doc_listworkers-tab.html";
var WORKER1_URL = "code_listworkers-worker1.js";
var WORKER2_URL = "code_listworkers-worker2.js";

add_task(async function test() {
  const tab = await addTab(TAB_URL);
  const target = await TargetFactory.forTab(tab);
  await target.attach();

  let { workers } = await listWorkers(target);
  is(workers.length, 0);

  executeSoon(() => {
    evalInTab(tab, "var worker1 = new Worker('" + WORKER1_URL + "');");
  });
  await waitForWorkerListChanged(target);

  ({ workers } = await listWorkers(target));
  is(workers.length, 1);
  is(workers[0].url, WORKER1_URL);

  executeSoon(() => {
    evalInTab(tab, "var worker2 = new Worker('" + WORKER2_URL + "');");
  });
  await waitForWorkerListChanged(target);

  ({ workers } = await listWorkers(target));
  is(workers.length, 2);
  is(workers[0].url, WORKER1_URL);
  is(workers[1].url, WORKER2_URL);

  executeSoon(() => {
    evalInTab(tab, "worker1.terminate()");
  });
  await waitForWorkerListChanged(target);

  ({ workers } = await listWorkers(target));
  is(workers.length, 1);
  is(workers[0].url, WORKER2_URL);

  executeSoon(() => {
    evalInTab(tab, "worker2.terminate()");
  });
  await waitForWorkerListChanged(target);

  ({ workers } = await listWorkers(target));
  is(workers.length, 0);

  await target.destroy();
  finish();
});
