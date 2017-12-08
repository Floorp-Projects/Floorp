// Test for bug 592641 - Image document doesn't show dimensions of cached images

// Globals
var testPath = "http://mochi.test:8888/browser/dom/html/test/";
var ctx = {loadsDone : 0};

// Entry point from Mochikit
function test() {

  waitForExplicitFinish();

  ctx.tab1 = BrowserTestUtils.addTab(gBrowser, testPath + "bug592641_img.jpg");
  ctx.tab1Browser = gBrowser.getBrowserForTab(ctx.tab1);
  BrowserTestUtils.browserLoaded(ctx.tab1Browser).then(load1Soon);
}

function checkTitle(title) {

  ctx.loadsDone++;
  ok(/^bug592641_img\.jpg \(JPEG Image, 1500\u00A0\u00D7\u00A01500 pixels\)/.test(title),
     "Title should be correct on load #" + ctx.loadsDone);
}

function load1Soon() {
  // onload is fired in OnStopDecode, so let's use executeSoon() to make sure
  // that any other OnStopDecode event handlers get the chance to fire first.
  executeSoon(load1Done);
}

function load1Done() {
  // Check the title
  var title = ctx.tab1Browser.contentTitle;
  checkTitle(title);

  // Try loading the same image in a new tab to make sure things work in
  // the cached case.
  ctx.tab2 = BrowserTestUtils.addTab(gBrowser, testPath + "bug592641_img.jpg");
  ctx.tab2Browser = gBrowser.getBrowserForTab(ctx.tab2);
  BrowserTestUtils.browserLoaded(ctx.tab2Browser).then(load2Soon);
}

function load2Soon() {
  // onload is fired in OnStopDecode, so let's use executeSoon() to make sure
  // that any other OnStopDecode event handlers get the chance to fire first.
  executeSoon(load2Done);
}

function load2Done() {
  // Check the title
  var title = ctx.tab2Browser.contentTitle;
  checkTitle(title);

  // Clean up
  gBrowser.removeTab(ctx.tab1);
  gBrowser.removeTab(ctx.tab2);

  // Test done
  finish();
}
