async function idbCheckFunc() {
  let factory;
  try {
    // in a worker, this resolves directly.
    factory = indexedDB;
  } catch (ex) {
    // in a frame-script, we need to pierce "content"
    factory = content.indexedDB;
  }
  try {
    console.log("opening db");
    const req = factory.open("db", 1);
    const result = await new Promise((resolve, reject) => {
      req.onerror = () => { resolve("error"); };
      // we expect the db to not exist and for created to resolve first
      req.onupgradeneeded = () => { resolve("created"); };
      // ...so this will lose the race
      req.onsuccess = (event) => {
        resolve("already-exists");
      };
    });
    const db = req.result;
    console.log("db req completed:", result);
    if (result !== "error") {
      db.close();
      console.log("deleting database");
      await new Promise((resolve, reject) => {
        const delReq = factory.deleteDatabase("db");
        delReq.onerror = reject;
        delReq.onsuccess = resolve;
      });
      console.log("deleted database");
    }
    return result;
  } catch (ex) {
    console.error("received error:", ex);
    return "exception";
  }
}

async function workerDriverFunc() {
  const resultPromise = idbCheckFunc();
  /* eslint-env worker */
  // (SharedWorker)
  if (!("postMessage" in self)) {
    addEventListener("connect", function(evt) {
      const port = evt.ports[0];
      resultPromise.then((result) => {
        console.log("worker test completed, postMessage-ing result:", result);
        port.postMessage({ idbResult: result });
      });
    });
  }
  const result = await resultPromise;
  // (DedicatedWorker)
  if ("postMessage" in self) {
    console.log("worker test completed, postMessage-ing result:", result);
    postMessage({ idbResult: result });
  }
}

const workerScript = `
${idbCheckFunc.toSource()}
(${workerDriverFunc.toSource()})();
`;
const workerScriptBlob = new Blob([workerScript]);

/**
 * This function is deployed via ContextTask.spawn and operates in a tab
 * frame script context.  Its job is to create the worker that will run the
 * idbCheckFunc and return the result to us.
 */
async function workerCheckDeployer({ srcBlob, workerType }) {
  let worker, port;
  const url = content.URL.createObjectURL(srcBlob);
  if (workerType === "dedicated") {
    worker = new content.Worker(url);
    port = worker;
  } else if (workerType === "shared") {
    worker = new content.SharedWorker(url);
    port = worker.port;
    port.start();
  } else {
    throw new Error("bad worker type!");
  }

  const result = await new Promise((resolve, reject) => {
    port.addEventListener("message", function(evt) {
      resolve(evt.data.idbResult);
    }, { once: true });
    worker.addEventListener("error", function(evt) {
      console.error("worker problem:", evt);
      reject(evt);
    });
  });
  console.log("worker completed test with result:", result);

  return result;
}

function checkTabWindowIDB(tab) {
  return ContentTask.spawn(tab.linkedBrowser, null, idbCheckFunc);
}

async function checkTabDedicatedWorkerIDB(tab) {
  return ContentTask.spawn(
    tab.linkedBrowser,
    {
      srcBlob: workerScriptBlob,
      workerType: "dedicated",
    },
    workerCheckDeployer);
}

async function checkTabSharedWorkerIDB(tab) {
  return ContentTask.spawn(
    tab.linkedBrowser,
    {
      srcBlob: workerScriptBlob,
      workerType: "shared",
    },
    workerCheckDeployer);
}

add_task(async function() {
  const pageUrl =
    "http://example.com/browser/dom/indexedDB/test/page_private_idb.html";

  let normalWin = await BrowserTestUtils.openNewBrowserWindow();
  let privateWin =
    await BrowserTestUtils.openNewBrowserWindow({private: true});

  let normalTab =
    await BrowserTestUtils.openNewForegroundTab(normalWin.gBrowser, pageUrl);
  let privateTab =
    await BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser, pageUrl);

  is(await checkTabWindowIDB(normalTab), "created",
     "IndexedDB works in a non-private-browsing page.");
  is(await checkTabWindowIDB(privateTab), "error",
     "IndexedDB does not work in a private-browsing page.");

  is(await checkTabDedicatedWorkerIDB(normalTab), "created",
     "IndexedDB works in a non-private-browsing Worker.");
  is(await checkTabDedicatedWorkerIDB(privateTab), "error",
     "IndexedDB does not work in a private-browsing Worker.");

  is(await checkTabSharedWorkerIDB(normalTab), "created",
     "IndexedDB works in a non-private-browsing SharedWorker.");
  is(await checkTabSharedWorkerIDB(privateTab), "error",
     "IndexedDB does not work in a private-browsing SharedWorker.");

  await BrowserTestUtils.closeWindow(normalWin);
  await BrowserTestUtils.closeWindow(privateWin);
});
