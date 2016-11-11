/*
 * Bug 1270678 - A test case to test does the favicon obey originAttributes.
 */
let { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
let {HttpServer} = Cu.import("resource://testing-common/httpd.js", {});

const USER_CONTEXTS = [
  "default",
  "personal",
  "work",
];

let gHttpServer = null;
let gUserContextId;
let gFaviconData;

function getIconFile() {
  new Promise(resolve => {
    NetUtil.asyncFetch({
      uri: "http://www.example.com/browser/browser/components/contextualidentity/test/browser/favicon-normal32.png",
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE_FAVICON
    }, function(inputStream, status) {
        let size = inputStream.available();
        gFaviconData = NetUtil.readInputStreamToString(inputStream, size);
        resolve();
    });
  });
}

function* openTabInUserContext(uri, userContextId) {
  // open the tab in the correct userContextId
  let tab = gBrowser.addTab(uri, {userContextId});

  // select tab and make sure its browser is focused
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

function loadIndexHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/html", false);
  let body = `
    <!DOCTYPE HTML>
      <html>
        <head>
          <meta charset='utf-8'>
          <title>Favicon Test</title>
        </head>
        <body>
          Favicon!!
        </body>
    </html>`;
  response.bodyOutputStream.write(body, body.length);
}

function loadFaviconHandler(metadata, response) {
  let expectedCookie = "userContext=" + USER_CONTEXTS[gUserContextId];

  if (metadata.hasHeader("Cookie")) {
    is(metadata.getHeader("Cookie"), expectedCookie, "The cookie has matched with the expected cookie.");
  } else {
    ok(false, "The request should have a cookie.");
  }

  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "image/png", false);
  response.bodyOutputStream.write(gFaviconData, gFaviconData.length);
}

add_task(function* setup() {
  // Make sure userContext is enabled.
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true]
    ]}, resolve);
  });

  // Create a http server for the image cache test.
  if (!gHttpServer) {
    gHttpServer = new HttpServer();
    gHttpServer.registerPathHandler('/', loadIndexHandler);
    gHttpServer.registerPathHandler('/favicon.png', loadFaviconHandler);
    gHttpServer.start(-1);
  }
});

registerCleanupFunction(() => {
  gHttpServer.stop(() => {
    gHttpServer = null;
  });
});

add_task(function* test() {
  waitForExplicitFinish();

  // First, get the icon data.
  yield getIconFile();

  let serverPort = gHttpServer.identity.primaryPort;
  let testURL = "http://localhost:" + serverPort + "/";
  let testFaviconURL = "http://localhost:" + serverPort + "/favicon.png";

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    gUserContextId = userContextId;

    // Load the page in 3 different contexts and set a cookie
    // which should only be visible in that context.

    // Open our tab in the given user context.
    let tabInfo = yield* openTabInUserContext(testURL, userContextId);

    // Write a cookie according to the userContext.
    yield ContentTask.spawn(tabInfo.browser, { userContext: USER_CONTEXTS[userContextId] }, function(arg) {
      content.document.cookie = "userContext=" + arg.userContext;
    });

    let pageURI = NetUtil.newURI(testURL);
    let favIconURI = NetUtil.newURI(testFaviconURL);

    yield new Promise(resolve => {
      PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, favIconURI,
        true, PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE, {
          onComplete() {
            resolve();
          },
        },
        tabInfo.browser.contentPrincipal);
    });

    yield BrowserTestUtils.removeTab(tabInfo.tab);
  }
});
