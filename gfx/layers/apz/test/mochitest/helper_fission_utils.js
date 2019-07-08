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
 * Returns a promise that will resolve if the `window` receives an event of the
 * given type that passes the given filter. Only the first matching message is
 * used. The filter must be a function (or null); it is called with the event
 * object and the call must return true to resolve the promise.
 */
function promiseOneEvent(eventType, filter) {
  return new Promise((resolve, reject) => {
    window.addEventListener(eventType, function listener(e) {
      let success = false;
      if (filter == null) {
        success = true;
      } else if (typeof filter == "function") {
        try {
          success = filter(e);
        } catch (ex) {
          dump(
            `ERROR: Filter passed to promiseOneEvent threw exception: ${ex}\n`
          );
          reject();
          return;
        }
      } else {
        dump(
          "ERROR: Filter passed to promiseOneEvent was neither null nor a function\n"
        );
        reject();
        return;
      }
      if (success) {
        window.removeEventListener(eventType, listener);
        resolve(e);
      }
    });
  });
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
        `WARNING: Calling loadOOPIFrame from ${
          window.location.href
        } so the iframe may not be OOP\n`
      );
      ok(false, "Current origin is not example.com:443");
    }

    let url =
      "https://example.com/browser/gfx/layers/apz/test/mochitest/" + iframePage;
    let loadPromise = promiseOneEvent("OOPIF:Load", function(e) {
      return typeof e.data.content == "string" && e.data.content == url;
    });
    let elem = document.getElementById(iframeElementId);
    elem.src = url;
    await loadPromise;
  };
}
