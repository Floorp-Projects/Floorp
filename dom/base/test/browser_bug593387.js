/*
 * Test for bug 593387
 * Loads a chrome document in a content docshell and then inserts a
 * X-Frame-Options: DENY iframe into the document and verifies that the document
 * loads. The policy we are enforcing is outlined here:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=593387#c17
 */

add_task(async function test() {
  // We have to disable CSP for this test otherwise the CSP of about:plugins will
  // block the dynamic frame creation.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.csp.enable", false],
      ["dom.security.skip_about_page_has_csp_assert", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:plugins" },
    async function(newBrowser) {
      // ---------------------------------------------------
      // Test 1: We load the about: page in the parent process, so this will work.
      await SpecialPowers.spawn(newBrowser, [], async function() {
        // Insert an iframe that specifies "X-Frame-Options: DENY" and verify
        // that it loads, since the top context is chrome
        var frame = content.document.createElement("iframe");
        frame.src =
          "http://mochi.test:8888/browser/dom/base/test/file_x-frame-options_page.sjs?testid=deny&xfo=deny";
        content.document.body.appendChild(frame);

        // wait till the iframe is load
        await new content.Promise(done => {
          frame.addEventListener(
            "load",
            function() {
              done();
            },
            { capture: true, once: true }
          );
        });

        await SpecialPowers.spawn(frame, [], () => {
          var testFrame = content.document.getElementById("test");
          Assert.equal(testFrame.tagName, "H1", "wrong element type");
          Assert.equal(testFrame.textContent, "deny", "wrong textContent");
        });
      });

      // ---------------------------------------------------
      // Test 2: Try the same with a content top-level context)

      await BrowserTestUtils.loadURI(newBrowser, "http://example.com/");
      await BrowserTestUtils.browserLoaded(newBrowser);

      let observerData = await SpecialPowers.spawn(
        newBrowser,
        [],
        async function() {
          var observerDeferred = {};
          observerDeferred.promise = new Promise(resolve => {
            observerDeferred.resolve = resolve;
          });

          // X-Frame-Options checks happen in the parent, hence we have to
          // proxy the csp violation notifications.
          SpecialPowers.registerObservers("xfo-on-violate-policy");

          function examiner() {
            SpecialPowers.addObserver(
              this,
              "specialpowers-xfo-on-violate-policy"
            );
          }
          examiner.prototype = {
            observe(subject, topic, data) {
              var asciiSpec = SpecialPowers.getPrivilegedProps(
                SpecialPowers.do_QueryInterface(subject, "nsIURI"),
                "asciiSpec"
              );

              myExaminer.remove();
              observerDeferred.resolve({ asciiSpec, topic, data });
            },
            remove() {
              SpecialPowers.removeObserver(
                this,
                "specialpowers-xfo-on-violate-policy"
              );
            },
          };
          let myExaminer = new examiner();

          var frame = content.document.createElement("iframe");
          frame.src =
            "http://mochi.test:8888/browser/dom/base/test/file_x-frame-options_page.sjs?testid=deny&xfo=deny";
          content.document.body.appendChild(frame);
          return observerDeferred.promise;
        }
      );
      is(
        observerData.asciiSpec,
        "http://mochi.test:8888/browser/dom/base/test/file_x-frame-options_page.sjs?testid=deny&xfo=deny",
        "correct subject"
      );
      ok(observerData.topic.endsWith("xfo-on-violate-policy"), "correct topic");
      is(observerData.data, "DENY", "correct data");
    }
  );
});
