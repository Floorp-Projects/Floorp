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
  return async function () {
    if (window.location.href.startsWith("https://example.com/")) {
      dump(
        `WARNING: Calling loadOOPIFrame from ${window.location.href} so the iframe may not be OOP\n`
      );
      ok(false, "Current origin is not example.com:443");
    }

    let url =
      "https://example.com/browser/gfx/layers/apz/test/mochitest/" + iframePage;
    let loadPromise = promiseOneEvent(window, "OOPIF:Load", function (e) {
      return typeof e.data.content == "string" && e.data.content == url;
    });
    let elem = document.getElementById(iframeElementId);
    elem.src = url;
    await loadPromise;
  };
}

/**
 * This is similar to the hitTest function in apz_test_utils.js, in that it
 * does a hit-test for a point and returns the result. The difference is that
 * in the fission world, the hit-test may land on an OOPIF, which means the
 * result information will be in the APZ test data for the OOPIF process. This
 * function checks both the current process and OOPIF process to see which one
 * got a hit result, and returns the result regardless of which process got it.
 * The caller is expected to check the layers id which will allow distinguishing
 * the two cases.
 */
async function fissionHitTest(point, iframeElement) {
  let get_iframe_compositor_test_data = function () {
    let utils = SpecialPowers.getDOMWindowUtils(window);
    return JSON.stringify(utils.getCompositorAPZTestData());
  };

  let utils = SpecialPowers.getDOMWindowUtils(window);

  // Get the test data before doing the actual hit-test, to get a baseline
  // of what we can ignore.
  let oldParentTestData = utils.getCompositorAPZTestData();
  let oldIframeTestData = JSON.parse(
    await FissionTestHelper.sendToOopif(
      iframeElement,
      `(${get_iframe_compositor_test_data})()`
    )
  );

  // Now do the hit-test
  dump(`Hit-testing point (${point.x}, ${point.y}) in fission context\n`);
  utils.sendMouseEvent(
    "MozMouseHittest",
    point.x,
    point.y,
    0,
    0,
    0,
    true,
    0,
    0,
    true,
    true
  );

  // Collect the new test data
  let newParentTestData = utils.getCompositorAPZTestData();
  let newIframeTestData = JSON.parse(
    await FissionTestHelper.sendToOopif(
      iframeElement,
      `(${get_iframe_compositor_test_data})()`
    )
  );

  // See which test data has new hit results
  let hitResultCount = function (testData) {
    return Object.keys(testData.hitResults).length;
  };

  let hitIframe =
    hitResultCount(newIframeTestData) > hitResultCount(oldIframeTestData);
  let hitParent =
    hitResultCount(newParentTestData) > hitResultCount(oldParentTestData);

  // Extract the results from the appropriate test data
  let lastHitResult = function (testData) {
    let lastHit =
      testData.hitResults[Object.keys(testData.hitResults).length - 1];
    return {
      hitInfo: lastHit.hitResult,
      scrollId: lastHit.scrollId,
      layersId: lastHit.layersId,
    };
  };
  if (hitIframe && hitParent) {
    throw new Error(
      "Both iframe and parent got hit-results, that is unexpected!"
    );
  } else if (hitIframe) {
    return lastHitResult(newIframeTestData);
  } else if (hitParent) {
    return lastHitResult(newParentTestData);
  } else {
    throw new Error(
      "Neither iframe nor parent got the hit-result, that is unexpected!"
    );
  }
}
