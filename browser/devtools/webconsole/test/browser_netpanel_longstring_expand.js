/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the network panel works with LongStringActors.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";
const TEST_IMG = "http://example.com/browser/browser/devtools/webconsole/test/test-image.png";

const TEST_IMG_BASE64 =
  "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfAhkiAAAAVRJREFU" +
  "OI2lk7FLw0AUxr+YpC1CBqcMWfsvCCLdXFzqEJCgDl1EQRGxg9AhSBEJONhFhG52UCuFDjq5dxD8" +
  "FwoO0qGDOBQkl7vLOeWa2EQDffDBvTu+373Hu1OEEJgntGgxGD6J+7fLXKbt5VNUyhsKAChRBQcP" +
  "FVFeWskFGH694mZroCQqCLlAwPxcgJBP254CmAD5B7C7dgHLMLF3uzoL4DQEod+Z5sP1FizDxGgy" +
  "BqfhLID9AahX29J89bwPFgMsSEAQglAf9WobhPpScbPXr4FQHyzIADTsDizDRMPuIOC+zEeTMZo9" +
  "BwH3EfAMACccbtfGaDKGZZg423yUZrdrg3EqxQlPr0BTdTR7joREN2uqnlBmCwW1hIJagtev4f3z" +
  "A16/JvfiigMSYyzqJXlw/XKUyOORMUaBor6YavgdjKa8xGOnidadmwtwsnMu18q83/kHSou+bFND" +
  "Dr4AAAAASUVORK5CYII=";

let testDriver;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testNetworkPanel);
  }, true);
}

function testNetworkPanel() {
  testDriver = testGen();
  testDriver.next();
}

function checkIsVisible(aPanel, aList) {
  for (let id in aList) {
    let node = aPanel.document.getElementById(id);
    let isVisible = aList[id];
    is(node.style.display, (isVisible ? "block" : "none"), id + " isVisible=" + isVisible);
  }
}

function checkNodeContent(aPanel, aId, aContent) {
  let node = aPanel.document.getElementById(aId);
  if (node == null) {
    ok(false, "Tried to access node " + aId + " that doesn't exist!");
  }
  else if (node.textContent.indexOf(aContent) != -1) {
    ok(true, "checking content of " + aId);
  }
  else {
    ok(false, "Got false value for " + aId + ": " + node.textContent + " doesn't have " + aContent);
  }
}

function checkNodeKeyValue(aPanel, aId, aKey, aValue) {
  let node = aPanel.document.getElementById(aId);

  let headers = node.querySelectorAll("th");
  for (let i = 0; i < headers.length; i++) {
    if (headers[i].textContent == (aKey + ":")) {
      is(headers[i].nextElementSibling.textContent, aValue,
         "checking content of " + aId + " for key " + aKey);
      return;
    }
  }

  ok(false, "content check failed for " + aId + ", key " + aKey);
}

function testGen() {
  let hud = HUDService.getHudByWindow(content);
  let filterBox = hud.ui.filterBox;

  let headerValue = (new Array(456)).join("fooz bar");
  let headerValueGrip = {
    type: "longString",
    initial: headerValue.substr(0, 123),
    length: headerValue.length,
    actor: "faktor",
    _fullString: headerValue,
  };

  let imageContentGrip = {
    type: "longString",
    initial: TEST_IMG_BASE64.substr(0, 143),
    length: TEST_IMG_BASE64.length,
    actor: "faktor2",
    _fullString: TEST_IMG_BASE64,
  };

  let postDataValue = (new Array(123)).join("post me");
  let postDataGrip = {
    type: "longString",
    initial: postDataValue.substr(0, 172),
    length: postDataValue.length,
    actor: "faktor3",
    _fullString: postDataValue,
  };

  let httpActivity = {
    updates: ["responseContent", "eventTimings"],
    discardRequestBody: false,
    discardResponseBody: false,
    startedDateTime: (new Date()).toISOString(),
    request: {
      url: TEST_IMG,
      method: "GET",
      cookies: [],
      headers: [
        { name: "foo", value: "bar" },
        { name: "loongstring", value: headerValueGrip },
      ],
      postData: { text: postDataGrip },
    },
    response: {
      httpVersion: "HTTP/3.14",
      status: 2012,
      statusText: "ddahl likes tacos :)",
      headers: [
        { name: "Content-Type", value: "image/png" },
      ],
      content: { mimeType: "image/png", text: imageContentGrip },
      cookies: [],
    },
    timings: { wait: 15, receive: 23 },
  };

  let networkPanel = hud.ui.openNetworkPanel(filterBox, httpActivity);

  is(filterBox._netPanel, networkPanel,
     "Network panel stored on the anchor object");

  networkPanel._onUpdate = function() {
    networkPanel._onUpdate = null;
    executeSoon(function() {
      testDriver.next();
    });
  };

  yield;

  info("test 1: check if a header value is expandable");

  checkIsVisible(networkPanel, {
    requestCookie: false,
    requestFormData: false,
    requestBody: false,
    requestBodyFetchLink: true,
    responseContainer: true,
    responseBody: false,
    responseNoBody: false,
    responseImage: true,
    responseImageCached: false,
    responseBodyFetchLink: true,
  });

  checkNodeKeyValue(networkPanel, "requestHeadersContent", "foo", "bar");
  checkNodeKeyValue(networkPanel, "requestHeadersContent", "loongstring",
                    headerValueGrip.initial + "[\u2026]");

  let webConsoleClient = networkPanel.webconsole.webConsoleClient;
  let longStringFn = webConsoleClient.longString;

  let expectedGrip = headerValueGrip;

  function longStringClientProvider(aLongString)
  {
    is(aLongString, expectedGrip,
       "longString grip is correct");

    return {
      initial: expectedGrip.initial,
      length: expectedGrip.length,
      substring: function(aStart, aEnd, aCallback) {
        is(aStart, expectedGrip.initial.length,
           "substring start is correct");
        is(aEnd, expectedGrip.length,
           "substring end is correct");

        executeSoon(function() {
          aCallback({
            substring: expectedGrip._fullString.substring(aStart, aEnd),
          });

          executeSoon(function() {
            testDriver.next();
          });
        });
      },
    };
  }

  webConsoleClient.longString = longStringClientProvider;

  let clickable = networkPanel.document
                  .querySelector("#requestHeadersContent .longStringEllipsis");
  ok(clickable, "long string ellipsis is shown");

  EventUtils.sendMouseEvent({ type: "mousedown"}, clickable,
                             networkPanel.document.defaultView);

  yield;

  clickable = networkPanel.document
              .querySelector("#requestHeadersContent .longStringEllipsis");
  ok(!clickable, "long string ellipsis is not shown");

  checkNodeKeyValue(networkPanel, "requestHeadersContent", "loongstring",
                    expectedGrip._fullString);

  info("test 2: check that response body image fetching works");
  expectedGrip = imageContentGrip;

  let imgNode = networkPanel.document.getElementById("responseImageNode");
  ok(!imgNode.getAttribute("src"), "no image is displayed");

  clickable = networkPanel.document.querySelector("#responseBodyFetchLink");
  EventUtils.sendMouseEvent({ type: "mousedown"}, clickable,
                             networkPanel.document.defaultView);

  yield;

  imgNode = networkPanel.document.getElementById("responseImageNode");
  is(imgNode.getAttribute("src"), "data:image/png;base64," + TEST_IMG_BASE64,
     "displayed image is correct");
  is(clickable.style.display, "none", "#responseBodyFetchLink is not visible");

  info("test 3: expand the request body");

  expectedGrip = postDataGrip;

  clickable = networkPanel.document.querySelector("#requestBodyFetchLink");
  EventUtils.sendMouseEvent({ type: "mousedown"}, clickable,
                             networkPanel.document.defaultView);
  yield;

  is(clickable.style.display, "none", "#requestBodyFetchLink is not visible");

  checkIsVisible(networkPanel, {
    requestBody: true,
    requestBodyFetchLink: false,
  });

  checkNodeContent(networkPanel, "requestBodyContent", expectedGrip._fullString);

  webConsoleClient.longString = longStringFn;

  networkPanel.panel.hidePopup();

  info("test 4: reponse body long text");

  httpActivity.response.content.mimeType = "text/plain";
  httpActivity.response.headers[0].value = "text/plain";

  expectedGrip = imageContentGrip;

  // Reset response.content.text to avoid caching of the full string.
  httpActivity.response.content.text = expectedGrip;

  networkPanel = hud.ui.openNetworkPanel(filterBox, httpActivity);
  is(filterBox._netPanel, networkPanel,
     "Network panel stored on httpActivity object");

  networkPanel._onUpdate = function() {
    networkPanel._onUpdate = null;
    executeSoon(function() {
      testDriver.next();
    });
  };

  yield;

  checkIsVisible(networkPanel, {
    requestCookie: false,
    requestFormData: false,
    requestBody: true,
    requestBodyFetchLink: false,
    responseContainer: true,
    responseBody: true,
    responseNoBody: false,
    responseImage: false,
    responseImageCached: false,
    responseBodyFetchLink: true,
  });

  checkNodeContent(networkPanel, "responseBodyContent", expectedGrip.initial);

  webConsoleClient.longString = longStringClientProvider;

  clickable = networkPanel.document.querySelector("#responseBodyFetchLink");
  EventUtils.sendMouseEvent({ type: "mousedown"}, clickable,
                             networkPanel.document.defaultView);

  yield;

  webConsoleClient.longString = longStringFn;
  is(clickable.style.display, "none", "#responseBodyFetchLink is not visible");
  checkNodeContent(networkPanel, "responseBodyContent", expectedGrip._fullString);

  networkPanel.panel.hidePopup();

  // All done!
  testDriver = null;
  executeSoon(finishTest);

  yield;
}
