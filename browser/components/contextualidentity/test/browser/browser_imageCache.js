let Cu = Components.utils;
let {HttpServer} = Cu.import("resource://testing-common/httpd.js", {});

const NUM_USER_CONTEXTS = 3;

let gHits = 0;

let server = new HttpServer();
server.registerPathHandler("/image.png", imageHandler);
server.registerPathHandler("/file.html", fileHandler);
server.start(-1);

let BASE_URI = "http://localhost:" + server.identity.primaryPort;
let IMAGE_URI = BASE_URI + "/image.png";
let FILE_URI = BASE_URI + "/file.html";

function imageHandler(metadata, response) {
  gHits++;
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/png", false);
  var body = "iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAIAAADZSiLoAAAAEUlEQVQImWP4z8AAQTAamQkAhpcI+DeMzFcAAAAASUVORK5CYII=";
  response.bodyOutputStream.write(body, body.length);
}

function fileHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  let body = `<html><body><image src=${IMAGE_URI}></body></html>`;
  response.bodyOutputStream.write(body, body.length);
}

add_task(async function setup() {
  // make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({"set": [["privacy.userContext.enabled", true]]});
});

// opens `uri' in a new tab with the provided userContextId and focuses it.
// returns the newly opened tab
async function openTabInUserContext(uri, userContextId) {
  // open the tab in the correct userContextId
  let tab = BrowserTestUtils.addTab(gBrowser, uri, {userContextId});

  // select tab and make sure its browser is focused
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return tab;
}

add_task(async function test() {
  for (let userContextId = 0; userContextId < NUM_USER_CONTEXTS; userContextId++) {
    let tab = await openTabInUserContext(FILE_URI, userContextId);
    gBrowser.removeTab(tab);
  }
  is(gHits, NUM_USER_CONTEXTS, "should get an image request for each user contexts");
});
