/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Julian Viereck <jviereck@mozilla.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that network log messages bring up the network panel.

const TEST_URI = "data:text/html;charset=utf-8,Web Console network logging tests";

const TEST_NETWORK_REQUEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-network-request.html";

const TEST_IMG = "http://example.com/browser/browser/devtools/webconsole/test/test-image.png";

const TEST_DATA_JSON_CONTENT =
  '{ id: "test JSON data", myArray: [ "foo", "bar", "baz", "biff" ] }';

let lastRequest = null;
let requestCallback = null;
let browser, hud;

function test()
{
  loadTab(TEST_URI).then((tab) => {
    browser = tab.browser;

    openConsole().then((aHud) => {
      hud = aHud;

      HUDService.lastFinishedRequest.callback = requestCallbackWrapper;

      executeSoon(testPageLoad);
    });
  });
}

function requestCallbackWrapper(aRequest)
{
  lastRequest = aRequest;

  hud.ui.webConsoleClient.getResponseContent(lastRequest.actor,
    function(aResponse) {
      lastRequest.response.content = aResponse.content;
      lastRequest.discardResponseBody = aResponse.contentDiscarded;

      hud.ui.webConsoleClient.getRequestPostData(lastRequest.actor,
        function(aResponse) {
          lastRequest.request.postData = aResponse.postData;
          lastRequest.discardRequestBody = aResponse.postDataDiscarded;

          if (requestCallback) {
            requestCallback();
          }
        });
    });
}

function testPageLoad()
{
  requestCallback = function() {
    // Check if page load was logged correctly.
    ok(lastRequest, "Page load was logged");

    is(lastRequest.request.url, TEST_NETWORK_REQUEST_URI,
      "Logged network entry is page load");
    is(lastRequest.request.method, "GET", "Method is correct");
    ok(!lastRequest.request.postData.text, "No request body was stored");
    ok(lastRequest.discardRequestBody, "Request body was discarded");
    ok(!lastRequest.response.content.text, "No response body was stored");
    ok(lastRequest.discardResponseBody || lastRequest.fromCache,
       "Response body was discarded or response came from the cache");

    lastRequest = null;
    requestCallback = null;
    executeSoon(testPageLoadBody);
  };

  content.location = TEST_NETWORK_REQUEST_URI;
}

function testPageLoadBody()
{
  // Turn on logging of request bodies and check again.
  hud.ui.setSaveRequestAndResponseBodies(true).then(() => {
    ok(hud.ui._saveRequestAndResponseBodies,
      "The saveRequestAndResponseBodies property was successfully set.");

    testPageLoadBodyAfterSettingUpdate();
  });
}

function testPageLoadBodyAfterSettingUpdate()
{
  let loaded = false;
  let requestCallbackInvoked = false;

  requestCallback = function() {
    ok(lastRequest, "Page load was logged again");
    ok(!lastRequest.discardResponseBody, "Response body was not discarded");
    is(lastRequest.response.content.text.indexOf("<!DOCTYPE HTML>"), 0,
      "Response body's beginning is okay");

    lastRequest = null;
    requestCallback = null;
    requestCallbackInvoked = true;

    if (loaded) {
      executeSoon(testXhrGet);
    }
  };

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    loaded = true;

    if (requestCallbackInvoked) {
      executeSoon(testXhrGet);
    }
  }, true);

  content.location.reload();
}

function testXhrGet()
{
  requestCallback = function() {
    ok(lastRequest, "testXhrGet() was logged");
    is(lastRequest.request.method, "GET", "Method is correct");
    ok(!lastRequest.request.postData.text, "No request body was sent");
    ok(!lastRequest.discardRequestBody, "Request body was not discarded");
    is(lastRequest.response.content.text, TEST_DATA_JSON_CONTENT,
      "Response is correct");

    lastRequest = null;
    requestCallback = null;
    executeSoon(testXhrPost);
  };

  // Start the XMLHttpRequest() GET test.
  content.wrappedJSObject.testXhrGet();
}

function testXhrPost()
{
  requestCallback = function() {
    ok(lastRequest, "testXhrPost() was logged");
    is(lastRequest.request.method, "POST", "Method is correct");
    is(lastRequest.request.postData.text, "Hello world!",
      "Request body was logged");
    is(lastRequest.response.content.text, TEST_DATA_JSON_CONTENT,
      "Response is correct");

    lastRequest = null;
    requestCallback = null;
    executeSoon(testFormSubmission);
  };

  // Start the XMLHttpRequest() POST test.
  content.wrappedJSObject.testXhrPost();
}

function testFormSubmission()
{
  // Start the form submission test. As the form is submitted, the page is
  // loaded again. Bind to the load event to catch when this is done.
  requestCallback = function() {
    ok(lastRequest, "testFormSubmission() was logged");
    is(lastRequest.request.method, "POST", "Method is correct");
    isnot(lastRequest.request.postData.text.
      indexOf("Content-Type: application/x-www-form-urlencoded"), -1,
      "Content-Type is correct");
    isnot(lastRequest.request.postData.text.
      indexOf("Content-Length: 20"), -1, "Content-length is correct");
    isnot(lastRequest.request.postData.text.
      indexOf("name=foo+bar&age=144"), -1, "Form data is correct");
    is(lastRequest.response.content.text.indexOf("<!DOCTYPE HTML>"), 0,
      "Response body's beginning is okay");

    executeSoon(testNetworkPanel);
  };
  ContentTask.spawn(gBrowser.selectedBrowser, { }, `function()
  {
    let form = content.document.querySelector("form");
    form.submit();
  }`);
}


function testNetworkPanel()
{
  // Open the NetworkPanel. The functionality of the NetworkPanel is tested
  // within separate test files.
  let networkPanel = hud.ui.openNetworkPanel(hud.ui.filterBox, lastRequest);

  networkPanel.panel.addEventListener("popupshown", function onPopupShown() {
    networkPanel.panel.removeEventListener("popupshown", onPopupShown, true);

    is(hud.ui.filterBox._netPanel, networkPanel,
       "Network panel stored on anchor node");
    ok(true, "NetworkPanel was opened");

    // All tests are done. Shutdown.
    networkPanel.panel.hidePopup();
    lastRequest = null;
    HUDService.lastFinishedRequest.callback = null;
    browser = requestCallback = hud = null;
    executeSoon(finishTest);
  }, true);
}

