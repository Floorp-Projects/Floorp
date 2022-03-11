/* import-globals-from ../../../testing/mochitest/tests/SimpleTest/SimpleTest.js */

function workerReply(port) {
  port.postMessage({ testTrialInterfaceExposed: !!self.TestTrialInterface });
}

if (
  self.SharedWorkerGlobalScope &&
  self instanceof self.SharedWorkerGlobalScope
) {
  self.addEventListener("connect", function(e) {
    const port = e.ports[0];
    workerReply(port);
    self.close();
  });
} else if (self.WorkerGlobalScope && self instanceof self.WorkerGlobalScope) {
  self.addEventListener("message", workerReply(self));
}

function assertTestTrialActive(shouldBeActive) {
  add_task(async function() {
    is(
      !!navigator.testTrialGatedAttribute,
      shouldBeActive,
      "Should match active status for Navigator.testTrialControlledAttribute"
    );
    is(
      !!self.TestTrialInterface,
      shouldBeActive,
      "Should match active status for TestTrialInterface"
    );
    if (shouldBeActive) {
      ok(
        new self.TestTrialInterface(),
        "Should be able to construct interface"
      );
    }

    {
      const worker = new Worker("common.js");
      await new Promise(resolve => {
        worker.addEventListener(
          "message",
          function(e) {
            is(
              e.data.testTrialInterfaceExposed,
              shouldBeActive,
              "Should work as expected in workers"
            );
            resolve();
          },
          { once: true }
        );
        worker.postMessage("ping");
      });
      worker.terminate();
    }

    {
      // We want a unique worker per page since the trial state depends on the
      // creator document.
      const worker = new SharedWorker("common.js", document.URL);
      const promise = new Promise(resolve => {
        worker.port.addEventListener(
          "message",
          function(e) {
            is(
              e.data.testTrialInterfaceExposed,
              shouldBeActive,
              "Should work as expected in shared workers"
            );
            resolve();
          },
          { once: true }
        );
      });
      worker.port.start();
      await promise;
    }

    // FIXME(emilio): Add more tests.
    //  * Stuff hanging off Window or Document (bug 1757935).
  });
}
