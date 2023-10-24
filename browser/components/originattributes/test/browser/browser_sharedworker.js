/**
 * Bug 1264593 - A test case for the shared worker by first party isolation.
 */

const TEST_DOMAIN = "https://example.net/";
const TEST_PATH =
  TEST_DOMAIN + "browser/browser/components/originattributes/test/browser/";
const TEST_PAGE = TEST_PATH + "file_sharedworker.html";

async function getResultFromSharedworker(aBrowser) {
  let response = await SpecialPowers.spawn(aBrowser, [], async function () {
    let worker = new content.SharedWorker(
      "file_shared.worker.js",
      "isolationSharedWorkerTest"
    );

    let result = await new content.Promise(resolve => {
      worker.port.onmessage = function (e) {
        content.document.getElementById("display").innerHTML = e.data;
        resolve(e.data);
      };
    });

    return result;
  });

  return response;
}

IsolationTestTools.runTests(TEST_PAGE, getResultFromSharedworker);
