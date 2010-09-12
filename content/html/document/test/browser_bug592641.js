// Test for bug 592641 - Image document doesn't show dimensions of cached images

// Globals
var testPath = "http://mochi.test:8888/browser/content/html/document/test/";
var ctx = {loadsDone : 0};

// Entry point from Mochikit
function test() {

  waitForExplicitFinish();

  ctx.tab1 = gBrowser.addTab(testPath + "bug592641_img.jpg");
  ctx.tab1Browser = gBrowser.getBrowserForTab(ctx.tab1);
  ctx.tab1Browser.addEventListener("load",
                                   // onload is fired in OnStopDecode, so let's
                                   // use executeSoon() to make sure that any
                                   // other OnStopDecode event handlers get the
                                   // chance to fire first.
                                   function() {executeSoon(load1Done);},
                                   true);
}

function checkTitle(title) {

  ctx.loadsDone++;
  ok(/^bug592641_img\.jpg \(JPEG Image, 1500x1500 pixels\)/.test(title),
     "Title should be correct on load #" + ctx.loadsDone);
}


function load1Done() {

  // Check the title
  var title = ctx.tab1Browser.contentWindow.document.title;
  checkTitle(title);

  // Try loading the same image in a new tab to make sure things work in
  // the cached case.
  ctx.tab2 = gBrowser.addTab(testPath + "bug592641_img.jpg");
  ctx.tab2Browser = gBrowser.getBrowserForTab(ctx.tab2);
  ctx.tab2Browser.addEventListener("load",
                                   // onload is fired in OnStopDecode, so let's
                                   // use executeSoon() to make sure that any
                                   // other OnStopDecode event handlers get the
                                   // chance to fire first.
                                   function() {executeSoon(load2Done);},
                                   true);


}

function load2Done() {

  // Check the title
  var title = ctx.tab2Browser.contentWindow.document.title;
  checkTitle(title);

  // Clean up
  gBrowser.removeTab(ctx.tab1);
  gBrowser.removeTab(ctx.tab2);

  // Test done
  finish();
}
