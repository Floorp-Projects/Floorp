const PREF_WC_REPORTER_ENABLED = "extensions.webcompat-reporter.enabled";
const PREF_WC_REPORTER_ENDPOINT = "extensions.webcompat-reporter.newIssueEndpoint";

const TEST_ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const TEST_PAGE = TEST_ROOT + "test.html";
const NEW_ISSUE_PAGE = TEST_ROOT + "webcompat.html";

function isButtonDisabled() {
  return document.getElementById("webcompat-reporter-button").disabled;
}
