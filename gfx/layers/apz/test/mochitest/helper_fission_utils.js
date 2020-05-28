// loadOOPIFrame expects apz_test_utils.js to be loaded as well, for promiseOneEvent.
/* import-globals-from apz_test_utils.js */

function fission_subtest_init() {
  // Silence SimpleTest warning about missing assertions by having it wait
  // indefinitely. We don't need to give it an explicit finish because the
  // entire window this test runs in will be closed after subtestDone is called.
  SimpleTest.waitForExplicitFinish();

  // This is the point at which we inject the ok, is, subtestDone, etc. functions
  // into this window. In particular this function should run after SimpleTest.js
  // is imported, otherwise SimpleTest.js will clobber the functions with its
  // own versions. This is implicitly enforced because if we call this function
  // before SimpleTest.js is imported, the above line will throw an exception.
  window.dispatchEvent(new Event("FissionTestHelper:Init"));
}

/**
 * Starts loading the given `iframePage` in the iframe element with the given
 * id, and waits for it to load.
 * Note that calling this function doesn't do the load directly; instead it
 * returns an async function which can be added to a thenable chain.
 */
function loadOOPIFrame(iframeElementId, iframePage) {
  return async function() {
    if (window.location.href.startsWith("https://example.com/")) {
      dump(
        `WARNING: Calling loadOOPIFrame from ${window.location.href} so the iframe may not be OOP\n`
      );
      ok(false, "Current origin is not example.com:443");
    }

    let url =
      "https://example.com/browser/gfx/layers/apz/test/mochitest/" + iframePage;
    let loadPromise = promiseOneEvent(window, "OOPIF:Load", function(e) {
      return typeof e.data.content == "string" && e.data.content == url;
    });
    let elem = document.getElementById(iframeElementId);
    elem.src = url;
    await loadPromise;
  };
}
