/**
 * Bug 1264567 - A test case for localStorage isolation.
 */

const TEST_PAGE = "http://mochi.test:8888/browser/browser/components/" +
                  "originattributes/test/browser/file_firstPartyBasic.html";

// Use a random key so we don't access it in later tests.
const key = Math.random().toString();

// Define the testing function
function doTest(aBrowser) {
  return ContentTask.spawn(aBrowser, key, function(contentKey) {
    let value = content.localStorage.getItem(contentKey);
    if (value === null) {
      // No value is found, so we create one.
      value = Math.random().toString();
      content.localStorage.setItem(contentKey, value);
    }
    return value;
  });
}

IsolationTestTools.runTests(TEST_PAGE, doTest);
