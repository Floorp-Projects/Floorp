/**
 * Bug 1264573 - A test case for blob url isolation.
 */

const TEST_PAGE =
  "http://mochi.test:8888/browser/browser/components/" +
  "originattributes/test/browser/file_firstPartyBasic.html";
const SCRIPT_WORKER_BLOBIFY = "blobify.worker.js";

function page_blobify(browser, input) {
  return SpecialPowers.spawn(browser, [input], function (contentInput) {
    return {
      blobURL: content.URL.createObjectURL(new content.Blob([contentInput])),
    };
  });
}

function page_deblobify(browser, blobURL) {
  return SpecialPowers.spawn(
    browser,
    [blobURL],
    async function (contentBlobURL) {
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

      let blob = await blobURLtoBlob(contentBlobURL);
      if (blob == "xhr error") {
        return "xhr error";
      }

      return blobToString(blob);
    }
  );
}

function workerIO(browser, what, message) {
  return SpecialPowers.spawn(
    browser,
    [
      {
        scriptFile: SCRIPT_WORKER_BLOBIFY,
        message: { message, what },
      },
    ],
    function (args) {
      if (!content.worker) {
        content.worker = new content.Worker(args.scriptFile);
      }
      let promise = new content.Promise(function (resolve) {
        let listenFunction = function (event) {
          content.worker.removeEventListener("message", listenFunction);
          resolve(event.data);
        };
        content.worker.addEventListener("message", listenFunction);
      });
      content.worker.postMessage(args.message);
      return promise;
    }
  );
}

let worker_blobify = (browser, input) => workerIO(browser, "blobify", input);
let worker_deblobify = (browser, blobURL) =>
  workerIO(browser, "deblobify", blobURL);

function doTest(blobify, deblobify) {
  let blobURL = null;
  return async function (browser) {
    if (blobURL === null) {
      let input = Math.random().toString();
      blobURL = await blobify(browser, input);
      return input;
    }
    let result = await deblobify(browser, blobURL);
    blobURL = null;
    return result;
  };
}

let tests = [];
for (let blobify of [page_blobify, worker_blobify]) {
  for (let deblobify of [page_deblobify, worker_deblobify]) {
    tests.push(doTest(blobify, deblobify));
  }
}

async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.partition.bloburl_per_partition_key", false],
      ["dom.security.https_first", false],
    ],
  });
}

IsolationTestTools.runTests(TEST_PAGE, tests, null, setup);
