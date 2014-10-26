/*
 * Test for bug 593387
 * Loads a chrome document in a content docshell and then inserts a
 * X-Frame-Options: DENY iframe into the document and verifies that the document
 * loads. The policy we are enforcing is outlined here:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=593387#c17
*/
var newBrowser;

function test() {
  waitForExplicitFinish();

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  newBrowser = gBrowser.getBrowserForTab(newTab);
  //alert(newBrowser.contentWindow);

  newBrowser.addEventListener("load", testXFOFrameInChrome, true);
  newBrowser.contentWindow.location = "chrome://global/content/mozilla.xhtml";
}

function testXFOFrameInChrome() {
  newBrowser.removeEventListener("load", testXFOFrameInChrome, true);

  // Insert an iframe that specifies "X-Frame-Options: DENY" and verify
  // that it loads, since the top context is chrome
  var frame = newBrowser.contentDocument.createElement("iframe");
  frame.src = "http://mochi.test:8888/tests/dom/base/test/file_x-frame-options_page.sjs?testid=deny&xfo=deny";
  frame.addEventListener("load", function() {
    frame.removeEventListener("load", arguments.callee, true);

    // Test that the frame loaded
    var test = this.contentDocument.getElementById("test");
    is(test.tagName, "H1", "wrong element type");
    is(test.textContent, "deny", "wrong textContent");
    
    // Run next test (try the same with a content top-level context)
    newBrowser.addEventListener("load", testXFOFrameInContent, true);
    newBrowser.contentWindow.location = "http://example.com/";  
  }, true);

  newBrowser.contentDocument.body.appendChild(frame);
}

function testXFOFrameInContent() {
  newBrowser.removeEventListener("load", testXFOFrameInContent, true);

  // Insert an iframe that specifies "X-Frame-Options: DENY" and verify that it
  // is blocked from loading since the top browsing context is another site
  var frame = newBrowser.contentDocument.createElement("iframe");
  frame.src = "http://mochi.test:8888/tests/dom/base/test/file_x-frame-options_page.sjs?testid=deny&xfo=deny";
  frame.addEventListener("load", function() {
    frame.removeEventListener("load", arguments.callee, true);

    // Test that the frame DID NOT load
    var test = this.contentDocument.getElementById("test");
    is(test, undefined, "should be about:blank");

    // Finalize the test
    gBrowser.removeCurrentTab();
    finish();
  }, true);

  newBrowser.contentDocument.body.appendChild(frame);
}
