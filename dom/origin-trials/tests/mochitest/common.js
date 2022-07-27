/* import-globals-from ../../../../testing/mochitest/tests/SimpleTest/SimpleTest.js */

// This would be a bit nicer with `self`, but Worklet doesn't have that, so
// `globalThis` it is, see https://github.com/whatwg/html/issues/7696
function workerReply(port) {
  port.postMessage({
    testTrialInterfaceExposed: !!globalThis.TestTrialInterface,
  });
}

if (
  globalThis.SharedWorkerGlobalScope &&
  globalThis instanceof globalThis.SharedWorkerGlobalScope
) {
  globalThis.addEventListener("connect", function(e) {
    const port = e.ports[0];
    workerReply(port);
  });
} else if (
  globalThis.WorkerGlobalScope &&
  globalThis instanceof globalThis.WorkerGlobalScope
) {
  workerReply(globalThis);
} else if (
  globalThis.WorkletGlobalScope &&
  globalThis instanceof globalThis.WorkletGlobalScope
) {
  class Processor extends AudioWorkletProcessor {
    constructor() {
      super();
      this.port.start();
      workerReply(this.port);
    }

    process(inputs, outputs, parameters) {
      // Do nothing, output silence
      return true;
    }
  }
  registerProcessor("test-processor", Processor);
}

function assertTestTrialActive(shouldBeActive) {
  add_task(async function() {
    info("Main thread test: " + document.URL);
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

    function promiseWorkerWorkletMessage(target, context) {
      info(`promiseWorkerWorkletMessage(${context})`);
      return new Promise(resolve => {
        target.addEventListener(
          "message",
          function(e) {
            is(
              e.data.testTrialInterfaceExposed,
              shouldBeActive,
              "Should work as expected in " + context
            );
            info(`got ${context} message`);
            resolve();
          },
          { once: true }
        );
      });
    }

    {
      info("Worker test");
      const worker = new Worker("common.js");
      await promiseWorkerWorkletMessage(worker, "worker");
      worker.terminate();
    }

    {
      info("SharedWorker test");
      // We want a unique worker per page since the trial state depends on the
      // creator document.
      const worker = new SharedWorker("common.js", document.URL);
      const promise = promiseWorkerWorkletMessage(worker.port, "shared worker");
      worker.port.start();
      await promise;
    }

    {
      info("AudioWorklet test");
      const audioContext = new AudioContext();
      await audioContext.audioWorklet.addModule("common.js");
      audioContext.resume();
      const workletNode = new AudioWorkletNode(audioContext, "test-processor");
      const promise = promiseWorkerWorkletMessage(workletNode.port, "worklet");
      workletNode.port.start();
      await promise;
      await audioContext.close();
    }

    // FIXME(emilio): Add more tests.
    //  * Stuff hanging off Window or Document (bug 1757935).
  });
}
