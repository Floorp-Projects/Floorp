var MAX_TOTAL_VIEWERS = "browser.sessionhistory.max_total_viewers";

var TAB1_URL = EXAMPLE_URL + "doc_WorkerActor.attach-tab1.html";
var TAB2_URL = EXAMPLE_URL + "doc_WorkerActor.attach-tab2.html";
var WORKER1_URL = "code_WorkerActor.attach-worker1.js";
var WORKER2_URL = "code_WorkerActor.attach-worker2.js";

function test() {
  Task.spawn(function* () {
    let oldMaxTotalViewers = SpecialPowers.getIntPref(MAX_TOTAL_VIEWERS);
    SpecialPowers.setIntPref(MAX_TOTAL_VIEWERS, 10);

    DebuggerServer.init();
    DebuggerServer.addBrowserActors();

    let client = new DebuggerClient(DebuggerServer.connectPipe());
    yield connect(client);

    let tab = yield addTab(TAB1_URL);
    let { tabs } = yield listTabs(client);
    let [, tabClient] = yield attachTab(client, findTab(tabs, TAB1_URL));
    yield listWorkers(tabClient);

    // If a page still has pending network requests, it will not be moved into
    // the bfcache. Consequently, we cannot use waitForWorkerListChanged here,
    // because the worker is not guaranteed to have finished loading when it is
    // registered. Instead, we have to wait for the promise returned by
    // createWorker in the tab to be resolved.
    yield createWorkerInTab(tab, WORKER1_URL);
    let { workers } = yield listWorkers(tabClient);
    let [, workerClient1] = yield attachWorker(tabClient,
                                               findWorker(workers, WORKER1_URL));
    is(workerClient1.isFrozen, false);

    executeSoon(() => {
      tab.linkedBrowser.loadURI(TAB2_URL);
    });
    yield waitForWorkerFreeze(workerClient1);
    is(workerClient1.isFrozen, true, "worker should be frozen");

    yield createWorkerInTab(tab, WORKER2_URL);
    ({ workers } = yield listWorkers(tabClient));
    let [, workerClient2] = yield attachWorker(tabClient,
                                               findWorker(workers, WORKER2_URL));
    is(workerClient2.isFrozen, false);

    executeSoon(() => {
      tab.linkedBrowser.contentWindow.history.back();
    });
    yield Promise.all([
      waitForWorkerFreeze(workerClient2),
      waitForWorkerThaw(workerClient1)
    ]);

    terminateWorkerInTab(tab, WORKER1_URL);
    yield waitForWorkerClose(workerClient1);

    yield close(client);
    SpecialPowers.setIntPref(MAX_TOTAL_VIEWERS, oldMaxTotalViewers);
    finish();
  });
}
