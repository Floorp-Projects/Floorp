/**
 * Bug 1312541 - A test case for document.cookie isolation.
 */

const TEST_PAGE = "http://mochi.test:8888/browser/browser/components/" +
                  "originattributes/test/browser/file_firstPartyBasic.html";

// Use a random key so we don't access it in later tests.
const key = "key" + Math.random().toString();
const re = new RegExp(key + "=([0-9\.]+)");

// Define the testing function
function* doTest(aBrowser) {
  return yield ContentTask.spawn(aBrowser, [key, re],
                                 function([contentKey, contentRe]) {
    let result = contentRe.exec(content.document.cookie);
    if (result) {
      return result[1];
    }
    // No value is found, so we create one.
    let value = Math.random().toString();
    content.document.cookie = contentKey + "=" + value;
    return value;
  });
}

registerCleanupFunction(() => {
    Services.cookies.removeAll();
});

IsolationTestTools.runTests(TEST_PAGE, doTest);
