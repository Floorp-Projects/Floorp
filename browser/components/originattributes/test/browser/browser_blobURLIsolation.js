/**
 * Bug 1264573 - A test case for blob url isolation.
 */

const TEST_PAGE = "http://mochi.test:8888/browser/browser/components/" +
                  "originattributes/test/browser/file_firstPartyBasic.html";
const SCRIPT_WORKER_BLOBIFY = "worker_blobify.js";
const SCRIPT_WORKER_DEBLOBIFY = "worker_deblobify.js";

function page_blobify(browser, input) {
  return ContentTask.spawn(browser, input, function(contentInput) {
    return { blobURL: content.URL.createObjectURL(new content.Blob([contentInput])) };
  });
}

function page_deblobify(browser, blobURL) {
  return ContentTask.spawn(browser, blobURL, function* (contentBlobURL) {
    if ("error" in contentBlobURL) {
      return contentBlobURL;
    }
    contentBlobURL = contentBlobURL.blobURL;

    function blobURLtoBlob(aBlobURL) {
      return new content.Promise(function (resolve) {
        let xhr = new content.XMLHttpRequest();
        xhr.open("GET", aBlobURL, true);
        xhr.onload = function () {
          resolve(xhr.response);
        };
        xhr.onerror = function () {
          resolve("xhr error");
        };
        xhr.responseType = "blob";
        xhr.send();
      });
    }

    function blobToString(blob) {
      return new content.Promise(function (resolve) {
        let fileReader = new content.FileReader();
        fileReader.onload = function () {
          resolve(fileReader.result);
        };
        fileReader.readAsText(blob);
      });
    }

    let blob = yield blobURLtoBlob(contentBlobURL);
    if (blob == "xhr error") {
      return "xhr error";
    }

    return yield blobToString(blob);
  });
}

function workerIO(browser, scriptFile, message) {
  return ContentTask.spawn(browser, {scriptFile, message}, function* (args) {
    let worker = new content.Worker(args.scriptFile);
    let promise = new content.Promise(function(resolve) {
      let listenFunction = function(event) {
        worker.removeEventListener("message", listenFunction, false);
        worker.terminate();
        resolve(event.data);
      };
      worker.addEventListener("message", listenFunction, false);
    });
    worker.postMessage(args.message);
    return yield promise;
  });
}

let worker_blobify = (browser, input) => workerIO(browser, SCRIPT_WORKER_BLOBIFY, input);
let worker_deblobify = (browser, blobURL) => workerIO(browser, SCRIPT_WORKER_DEBLOBIFY, blobURL);

function doTest(blobify, deblobify) {
  let blobURL = null;
  return function* (browser) {
    if (blobURL === null) {
      let input = Math.random().toString();
      blobURL = yield blobify(browser, input);
      return input;
    }
    let result = yield deblobify(browser, blobURL);
    blobURL = null;
    return result;
  }
}

let tests = [];
for (let blobify of [page_blobify, worker_blobify]) {
  for (let deblobify of [page_deblobify, worker_deblobify]) {
    tests.push(doTest(blobify, deblobify));
  }
}

IsolationTestTools.runTests(TEST_PAGE, tests);
