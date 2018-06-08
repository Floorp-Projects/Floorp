var TAB_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerTargetActor.attachThread-worker.js";

function test() {
  Task.spawn(function* () {
    DebuggerServer.init();
    DebuggerServer.registerAllActors();

    let client1 = new DebuggerClient(DebuggerServer.connectPipe());
    yield connect(client1);
    let client2 = new DebuggerClient(DebuggerServer.connectPipe());
    yield connect(client2);

    let tab = yield addTab(TAB_URL);
    let { tabs: tabs1 } = yield listTabs(client1);
    let [, tabClient1] = yield attachTab(client1, findTab(tabs1, TAB_URL));
    let { tabs: tabs2 } = yield listTabs(client2);
    let [, tabClient2] = yield attachTab(client2, findTab(tabs2, TAB_URL));

    yield listWorkers(tabClient1);
    yield listWorkers(tabClient2);
    yield createWorkerInTab(tab, WORKER_URL);
    let { workers: workers1 } = yield listWorkers(tabClient1);
    let [, workerClient1] = yield attachWorker(tabClient1,
                                               findWorker(workers1, WORKER_URL));
    let { workers: workers2 } = yield listWorkers(tabClient2);
    let [, workerClient2] = yield attachWorker(tabClient2,
                                               findWorker(workers2, WORKER_URL));

    let location = { line: 5 };

    let [, threadClient1] = yield attachThread(workerClient1);
    let sources1 = yield getSources(threadClient1);
    let sourceClient1 = threadClient1.source(findSource(sources1,
                                                        EXAMPLE_URL + WORKER_URL));
    let [, breakpointClient1] = yield setBreakpoint(sourceClient1, location);
    yield resume(threadClient1);

    let [, threadClient2] = yield attachThread(workerClient2);
    let sources2 = yield getSources(threadClient2);
    let sourceClient2 = threadClient2.source(findSource(sources2,
                                                        EXAMPLE_URL + WORKER_URL));
    let [, breakpointClient2] = yield setBreakpoint(sourceClient2, location);
    yield resume(threadClient2);

    let packet = yield source(sourceClient1);
    let text = (yield new Promise(function (resolve) {
      let request = new XMLHttpRequest();
      request.open("GET", EXAMPLE_URL + WORKER_URL, true);
      request.send();
      request.onload = function () {
        resolve(request.responseText);
      };
    }));
    is(packet.source, text);

    postMessageToWorkerInTab(tab, WORKER_URL, "ping");
    yield Promise.all([
      waitForPause(threadClient1).then((packet) => {
        is(packet.type, "paused");
        let why = packet.why;
        is(why.type, "breakpoint");
        is(why.actors.length, 1);
        is(why.actors[0], breakpointClient1.actor);
        let frame = packet.frame;
        let where = frame.where;
        is(where.source.actor, sourceClient1.actor);
        is(where.line, location.line);
        let variables = frame.environment.bindings.variables;
        is(variables.a.value, 1);
        is(variables.b.value.type, "undefined");
        is(variables.c.value.type, "undefined");
        return resume(threadClient1);
      }),
      waitForPause(threadClient2).then((packet) => {
        is(packet.type, "paused");
        let why = packet.why;
        is(why.type, "breakpoint");
        is(why.actors.length, 1);
        is(why.actors[0], breakpointClient2.actor);
        let frame = packet.frame;
        let where = frame.where;
        is(where.source.actor, sourceClient2.actor);
        is(where.line, location.line);
        let variables = frame.environment.bindings.variables;
        is(variables.a.value, 1);
        is(variables.b.value.type, "undefined");
        is(variables.c.value.type, "undefined");
        return resume(threadClient2);
      }),
    ]);

    terminateWorkerInTab(tab, WORKER_URL);
    yield waitForWorkerClose(workerClient1);
    yield waitForWorkerClose(workerClient2);
    yield close(client1);
    yield close(client2);
    finish();
  });
}
