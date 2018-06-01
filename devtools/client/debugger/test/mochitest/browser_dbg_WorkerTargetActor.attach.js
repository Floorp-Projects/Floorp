var MAX_TOTAL_VIEWERS = "browser.sessionhistory.max_total_viewers";

var TAB1_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attach-tab1.html";
var TAB2_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attach-tab2.html";
var WORKER1_URL = "code_WorkerTargetActor.attach-worker1.js";
var WORKER2_URL = "code_WorkerTargetActor.attach-worker2.js";

function test() {
  Task.spawn(function* () {
    let oldMaxTotalViewers = SpecialPowers.getIntPref(MAX_TOTAL_VIEWERS);
    SpecialPowers.setIntPref(MAX_TOTAL_VIEWERS, 10);

    DebuggerServer.init();
    DebuggerServer.registerAllActors();

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
    is(workerClient1.isClosed, false, "worker in tab 1 should not be closed");

    executeSoon(() => {
      tab.linkedBrowser.loadURI(TAB2_URL);
    });
    yield waitForWorkerClose(workerClient1);
    is(workerClient1.isClosed, true, "worker in tab 1 should be closed");

    yield createWorkerInTab(tab, WORKER2_URL);
    ({ workers } = yield listWorkers(tabClient));
    let [, workerClient2] = yield attachWorker(tabClient,
                                               findWorker(workers, WORKER2_URL));
    is(workerClient2.isClosed, false, "worker in tab 2 should not be closed");

    executeSoon(() => {
      tab.linkedBrowser.goBack();
    });
    yield waitForWorkerClose(workerClient2);
    is(workerClient2.isClosed, true, "worker in tab 2 should be closed");

    ({ workers } = yield listWorkers(tabClient));
    [, workerClient1] = yield attachWorker(tabClient,
                                           findWorker(workers, WORKER1_URL));
    is(workerClient1.isClosed, false, "worker in tab 1 should not be closed");

    yield close(client);
    SpecialPowers.setIntPref(MAX_TOTAL_VIEWERS, oldMaxTotalViewers);
    finish();
  });
}
