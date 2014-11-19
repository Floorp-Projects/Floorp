// Common code for the iframes used by bug704320_preload.

var loadCount = 0;

// Called by the various onload handlers to indicate that a resource has
// been fully loaded.  We require three loads to complete (img, script,
// link) for this test.
function incrementLoad(tag) {
  loadCount++;
  if (loadCount == 3) {
    window.parent.postMessage("childLoadComplete", window.location.origin);
  } else if (loadCount > 3) {
    document.write("<h1>Too Many Load Events!</h1>");
    window.parent.postMessage("childOverload", window.location.origin);
  }
}

// This is same as incrementLoad, but the caller passes in the loadCount.
function incrementLoad2(tag, expectedLoadCount) {
  loadCount++;
  if (loadCount == expectedLoadCount) {
    window.parent.postMessage("childLoadComplete", window.location.origin);
  } else if (loadCount > expectedLoadCount) {
    document.write("<h1>Too Many Load Events!</h1>");
    window.parent.postMessage("childOverload", window.location.origin);
  }
}

// in case something fails to load, cause the test to fail.
function postfail(msg) {
  window.parent.postMessage("fail-" + msg, window.location.origin);
}


