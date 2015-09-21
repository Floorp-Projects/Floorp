var TAB_URL = EXAMPLE_URL + "doc_listworkers-tab.html";
var WORKER1_URL = "code_listworkers-worker1.js";
var WORKER2_URL = "code_listworkers-worker2.js";

function test() {
  Task.spawn(function* () {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();

    let client = new DebuggerClient(DebuggerServer.connectPipe());
    yield connect(client);

    let tab = yield addTab(TAB_URL);
    let { tabs } = yield listTabs(client);
    let [, tabClient] = yield attachTab(client, findTab(tabs, TAB_URL));

    let { workers } = yield listWorkers(tabClient);
    is(workers.length, 0);

    executeSoon(() => {
      evalInTab(tab, "var worker1 = new Worker('" + WORKER1_URL + "');");
    });
    yield waitForWorkerListChanged(tabClient);

    ({ workers } = yield listWorkers(tabClient));
    is(workers.length, 1);
    is(workers[0].url, WORKER1_URL);

    executeSoon(() => {
      evalInTab(tab, "var worker2 = new Worker('" + WORKER2_URL + "');");
    });
    yield waitForWorkerListChanged(tabClient);

    ({ workers } = yield listWorkers(tabClient));
    is(workers.length, 2);
    is(workers[0].url, WORKER1_URL);
    is(workers[1].url, WORKER2_URL);

    executeSoon(() => {
      evalInTab(tab, "worker1.terminate()");
    });
    yield waitForWorkerListChanged(tabClient);

    ({ workers } = yield listWorkers(tabClient));
    is(workers.length, 1);
    is(workers[0].url, WORKER2_URL);

    executeSoon(() => {
      evalInTab(tab, "worker2.terminate()");
    });
    yield waitForWorkerListChanged(tabClient);

    ({ workers } = yield listWorkers(tabClient));
    is(workers.length, 0);

    yield close(client);
    finish();
  });
}
