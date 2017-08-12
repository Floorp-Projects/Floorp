const PREF_WC_REPORTER_ENABLED = "extensions.webcompat-reporter.enabled";
const PREF_WC_REPORTER_ENDPOINT = "extensions.webcompat-reporter.newIssueEndpoint";

const TEST_ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const TEST_PAGE = TEST_ROOT + "test.html";
const NEW_ISSUE_PAGE = TEST_ROOT + "webcompat.html";

const WC_PAGE_ACTION_ID = "pageAction-panel-webcompat-reporter-button";

function isButtonDisabled() {
  return document.getElementById(WC_PAGE_ACTION_ID).disabled;
}

function openPageActions() {
  var event = new MouseEvent("click");
  BrowserPageActions.mainButtonClicked(event);
}
