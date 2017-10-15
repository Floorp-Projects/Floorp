/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

XPCOMUtils.defineLazyModuleGetter(this, "AboutHomeUtils",
  "resource:///modules/AboutHome.jsm");

ignoreAllUncaughtExceptions();
var gRightsVersion = Services.prefs.getIntPref("browser.rights.version");

add_task(async function setup() {
  // The following prefs would affect tests so make sure to disable them
  // before any tests start.
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.newtabpage.activity-stream.aboutHome.enabled", false],
    ["browser.onboarding.enabled", false],
    ["network.cookie.cookieBehavior", 0],
    ["network.cookie.lifetimePolicy", 0],
    ["browser.rights.override", true],
    [`browser.rights.${gRightsVersion}.shown`, false]
  ]});
});

add_task(async function() {
  info("Check that clearing cookies does not clear storage");

  await withSnippetsMap(
    () => {
      Cc["@mozilla.org/observer-service;1"]
        .getService(Ci.nsIObserverService)
        .notifyObservers(null, "cookie-changed", "cleared");
    },
    function() {
      isnot(content.gSnippetsMap.get("snippets-last-update"), null,
            "snippets-last-update should have a value");
    });
});

add_task(async function() {
  info("Check default snippets are shown");

  await withSnippetsMap(null, function() {
    let doc = content.document;
    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    is(snippetsElt.getElementsByTagName("span").length, 1,
       "A default snippet is present.");
  });
});

add_task(async function() {
  info("Check default snippets are shown if snippets are invalid xml");

  await withSnippetsMap(
    // This must set some incorrect xhtml code.
    snippetsMap => snippetsMap.set("snippets", "<p><b></p></b>"),
    function() {
      let doc = content.document;
      let snippetsElt = doc.getElementById("snippets");
      ok(snippetsElt, "Found snippets element");
      is(snippetsElt.getElementsByTagName("span").length, 1,
         "A default snippet is present.");

      content.gSnippetsMap.delete("snippets");
    });
});

add_task(async function() {
  info("Check snippets map is cleared if cached version is old");

  await withSnippetsMap(
    snippetsMap => {
      snippetsMap.set("snippets", "test");
      snippetsMap.set("snippets-cached-version", 0);
    },
    function() {
      let snippetsMap = content.gSnippetsMap;
      ok(!snippetsMap.has("snippets"), "snippets have been properly cleared");
      ok(!snippetsMap.has("snippets-cached-version"),
         "cached-version has been properly cleared");
    });
});

add_task(async function() {
  info("Check cached snippets are shown if cached version is current");

  await withSnippetsMap(
    snippetsMap => snippetsMap.set("snippets", "test"),
    function(args) {
      let doc = content.document;
      let snippetsMap = content.gSnippetsMap;

      let snippetsElt = doc.getElementById("snippets");
      ok(snippetsElt, "Found snippets element");
      is(snippetsElt.innerHTML, "test", "Cached snippet is present.");

      is(snippetsMap.get("snippets"), "test", "snippets still cached");
      is(snippetsMap.get("snippets-cached-version"),
         args.expectedVersion,
         "cached-version is correct");
      ok(snippetsMap.has("snippets-last-update"), "last-update still exists");
    }, { expectedVersion: AboutHomeUtils.snippetsVersion });
});

add_task(async function() {
  info("Check if the 'Know Your Rights' default snippet is shown when " +
    "'browser.rights.override' pref is set and that its link works");

  Services.prefs.setBoolPref("browser.rights.override", false);

  ok(AboutHomeUtils.showKnowYourRights, "AboutHomeUtils.showKnowYourRights should be TRUE");

  await withSnippetsMap(null, function() {
    let doc = content.document;
    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    let linkEl = snippetsElt.querySelector("a");
    is(linkEl.href, "about:rights", "Snippet link is present.");
  }, null, async function() {
    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, "about:rights");
    await BrowserTestUtils.synthesizeMouseAtCenter("a[href='about:rights']", {
      button: 0
    }, gBrowser.selectedBrowser);
    await loadPromise;
    is(gBrowser.currentURI.spec, "about:rights", "about:rights should have opened.");
  });
});

add_task(async function() {
  info("Check if the 'Know Your Rights' default snippet is NOT shown when " +
    "'browser.rights.override' pref is NOT set");

  Services.prefs.setBoolPref("browser.rights.override", true);

  let rightsData = AboutHomeUtils.knowYourRightsData;
  ok(!rightsData, "AboutHomeUtils.knowYourRightsData should be FALSE");

  await withSnippetsMap(null, function() {
    let doc = content.document;
    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    ok(snippetsElt.getElementsByTagName("a")[0].href != "about:rights",
      "Snippet link should not point to about:rights.");
  });
});

/**
 * Cleans up snippets and ensures that by default we don't try to check for
 * remote snippets since that may cause network bustage or slowness.
 *
 * @param aSetupFn
 *        The setup function to be run.
 * @param testFn
 *        the content task to run
 * @param testArgs (optional)
 *        the parameters to pass to the content task
 * @param parentFn (optional)
 *        the function to run in the parent after the content task has completed.
 * @return {Promise} resolved when the snippets are ready.  Gets the snippets map.
 */
async function withSnippetsMap(setupFn, testFn, testArgs = null, parentFn = null) {
  let setupFnSource;
  if (setupFn) {
    setupFnSource = setupFn.toSource();
  }

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, async function(browser) {
    let promiseAfterLocationChange = () => {
      return ContentTask.spawn(browser, {
        setupFnSource,
        version: AboutHomeUtils.snippetsVersion,
      }, async function(args) {
        return new Promise(resolve => {
          let document = content.document;
          // We're not using Promise-based listeners, because they resolve asynchronously.
          // The snippets test setup code relies on synchronous behaviour here.
          document.addEventListener("AboutHomeLoadSnippets", function() {
            let updateSnippets;
            if (args.setupFnSource) {
              // eslint-disable-next-line no-eval
              updateSnippets = eval(`(() => (${args.setupFnSource}))()`);
            }

            content.wrappedJSObject.ensureSnippetsMapThen(snippetsMap => {
              snippetsMap = Cu.waiveXrays(snippetsMap);
              info("Got snippets map: " +
                   "{ last-update: " + snippetsMap.get("snippets-last-update") +
                   ", cached-version: " + snippetsMap.get("snippets-cached-version") +
                   " }");
              // Don't try to update.
              snippetsMap.set("snippets-last-update", Date.now());
              snippetsMap.set("snippets-cached-version", args.version);
              // Clear snippets.
              snippetsMap.delete("snippets");

              if (updateSnippets) {
                updateSnippets(snippetsMap);
              }

              // Tack it to the global object
              content.gSnippetsMap = snippetsMap;

              resolve();
            });
          }, {once: true});
        });
      });
    };

    // We'd like to listen to the 'AboutHomeLoadSnippets' event on a fresh
    // document as soon as technically possible, so we use webProgress.
    let promise = new Promise(resolve => {
      let wpl = {
        onLocationChange() {
          gBrowser.removeProgressListener(wpl);
          // Phase 2: retrieving the snippets map is the next promise on our agenda.
          promiseAfterLocationChange().then(resolve);
        },
        onProgressChange() {},
        onStatusChange() {},
        onSecurityChange() {}
      };
      gBrowser.addProgressListener(wpl);
    });

    // Set the URL to 'about:home' here to allow capturing the 'AboutHomeLoadSnippets'
    // event.
    browser.loadURI("about:home");
    // Wait for LocationChange.
    await promise;

    await ContentTask.spawn(browser, testArgs, testFn);
    if (parentFn) {
      await parentFn();
    }
  });
}

