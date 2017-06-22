/*
 * Test for bug 593387
 * Loads a chrome document in a content docshell and then inserts a
 * X-Frame-Options: DENY iframe into the document and verifies that the document
 * loads. The policy we are enforcing is outlined here:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=593387#c17
*/

add_task(async function test() {
  await BrowserTestUtils.withNewTab({ gBrowser,
                                      url: "chrome://global/content/mozilla.xhtml" },
                                     async function(newBrowser) {
    // NB: We load the chrome:// page in the parent process.
    await testXFOFrameInChrome(newBrowser);

    // Run next test (try the same with a content top-level context)
    await BrowserTestUtils.loadURI(newBrowser, "http://example.com/");
    await BrowserTestUtils.browserLoaded(newBrowser);

    await ContentTask.spawn(newBrowser, null, testXFOFrameInContent);
  });
});

function testXFOFrameInChrome(newBrowser) {
  // Insert an iframe that specifies "X-Frame-Options: DENY" and verify
  // that it loads, since the top context is chrome
  var deferred = {};
  deferred.promise = new Promise((resolve) => {
    deferred.resolve = resolve;
  });

  var frame = newBrowser.contentDocument.createElement("iframe");
  frame.src = "http://mochi.test:8888/tests/dom/base/test/file_x-frame-options_page.sjs?testid=deny&xfo=deny";
  frame.addEventListener("load", function() {
    // Test that the frame loaded
    var test = this.contentDocument.getElementById("test");
    is(test.tagName, "H1", "wrong element type");
    is(test.textContent, "deny", "wrong textContent");
    deferred.resolve();
  }, {capture: true, once: true});

  newBrowser.contentDocument.body.appendChild(frame);
  return deferred.promise;
}

function testXFOFrameInContent(newBrowser) {
  // Insert an iframe that specifies "X-Frame-Options: DENY" and verify that it
  // is blocked from loading since the top browsing context is another site
  var deferred = {};
  deferred.promise = new Promise((resolve) => {
    deferred.resolve = resolve;
  });

  var frame = content.document.createElement("iframe");
  frame.src = "http://mochi.test:8888/tests/dom/base/test/file_x-frame-options_page.sjs?testid=deny&xfo=deny";
  frame.addEventListener("load", function() {
    // Test that the frame DID NOT load
    var test = this.contentDocument.getElementById("test");
    Assert.equal(test, null, "should be about:blank");

    deferred.resolve();
  }, {capture: true, once: true});

  content.document.body.appendChild(frame);
  return deferred.promise;
}
