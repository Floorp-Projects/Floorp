/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);
const { SearchUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchUtils.sys.mjs"
);
const { CustomizableUITestUtils } = ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

add_task(async function test_setup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

// |shouldWork| should be true if opensearch is expected to work and false if
// it is not.
async function test_opensearch(shouldWork) {
  let searchBar = BrowserSearch.searchBar;

  let rootDir = getRootDirectory(gTestPath);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    rootDir + "opensearch.html"
  );
  let searchPopup = document.getElementById("PopupSearchAutoComplete");
  let promiseSearchPopupShown = BrowserTestUtils.waitForEvent(
    searchPopup,
    "popupshown"
  );
  let searchBarButton = searchBar.querySelector(".searchbar-search-button");

  searchBarButton.click();
  await promiseSearchPopupShown;
  let oneOffsContainer = searchPopup.searchOneOffsContainer;
  let engineElement = oneOffsContainer.querySelector(
    ".searchbar-engine-one-off-add-engine"
  );
  if (shouldWork) {
    ok(engineElement, "There should be search engines available to add");
    ok(
      searchBar.getAttribute("addengines"),
      "Search bar should have addengines attribute"
    );
  } else {
    is(
      engineElement,
      null,
      "There should be no search engines available to add"
    );
    ok(
      !searchBar.getAttribute("addengines"),
      "Search bar should not have addengines attribute"
    );
  }
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function test_install_and_set_default() {
  // Make sure we are starting in an expected state to avoid false positive
  // test results.
  isnot(
    (await Services.search.getDefault()).name,
    "MozSearch",
    "Default search engine should not be MozSearch when test starts"
  );
  is(
    Services.search.getEngineByName("Foo"),
    null,
    'Engine "Foo" should not be present when test starts'
  );

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "MozSearch",
            URLTemplate: "http://example.com/?q={searchTerms}",
          },
        ],
        Default: "MozSearch",
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  // If this passes, it means that the new search engine was properly installed
  // *and* was properly set as the default.
  is(
    (await Services.search.getDefault()).name,
    "MozSearch",
    "Specified search engine should be the default"
  );

  // Clean up
  await Services.search.removeEngine(await Services.search.getDefault());
  EnterprisePolicyTesting.resetRunOnceState();
});

add_task(async function test_install_and_set_default_private() {
  // Make sure we are starting in an expected state to avoid false positive
  // test results.
  isnot(
    (await Services.search.getDefaultPrivate()).name,
    "MozSearch",
    "Default search engine should not be MozSearch when test starts"
  );
  is(
    Services.search.getEngineByName("Foo"),
    null,
    'Engine "Foo" should not be present when test starts'
  );

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "MozSearch",
            URLTemplate: "http://example.com/?q={searchTerms}",
          },
        ],
        DefaultPrivate: "MozSearch",
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  // If this passes, it means that the new search engine was properly installed
  // *and* was properly set as the default.
  is(
    (await Services.search.getDefaultPrivate()).name,
    "MozSearch",
    "Specified search engine should be the default private engine"
  );

  // Clean up
  await Services.search.removeEngine(await Services.search.getDefaultPrivate());
  EnterprisePolicyTesting.resetRunOnceState();
});

// Same as the last test, but with "PreventInstalls" set to true to make sure
// it does not prevent search engines from being installed properly
add_task(async function test_install_and_set_default_prevent_installs() {
  isnot(
    (await Services.search.getDefault()).name,
    "MozSearch",
    "Default search engine should not be MozSearch when test starts"
  );
  is(
    Services.search.getEngineByName("Foo"),
    null,
    'Engine "Foo" should not be present when test starts'
  );

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "MozSearch",
            URLTemplate: "http://example.com/?q={searchTerms}",
          },
        ],
        Default: "MozSearch",
        PreventInstalls: true,
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  is(
    (await Services.search.getDefault()).name,
    "MozSearch",
    "Specified search engine should be the default"
  );

  // Clean up
  await Services.search.removeEngine(await Services.search.getDefault());
  EnterprisePolicyTesting.resetRunOnceState();
});

add_task(async function test_opensearch_works() {
  // Clear out policies so we can test with no policies applied
  await setupPolicyEngineWithJson({
    policies: {},
  });
  // Ensure that opensearch works before we make sure that it can be properly
  // disabled
  await test_opensearch(true);
});

add_task(async function setup_prevent_installs() {
  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        PreventInstalls: true,
      },
    },
  });
});

add_task(async function test_prevent_install_ui() {
  // Check that about:preferences does not prompt user to install search engines
  // if that feature is disabled
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#search"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    let linkContainer = content.document.getElementById("addEnginesBox");
    if (!linkContainer.hidden) {
      await new Promise(resolve => {
        let mut = new linkContainer.ownerGlobal.MutationObserver(mutations => {
          mut.disconnect();
          resolve();
        });
        mut.observe(linkContainer, { attributeFilter: ["hidden"] });
      });
    }
    is(
      linkContainer.hidden,
      true,
      '"Find more search engines" link should be hidden'
    );
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_opensearch_disabled() {
  // Check that search engines cannot be added via opensearch
  await test_opensearch(false);
});

add_task(async function test_install_and_remove() {
  let iconURL =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";

  is(
    Services.search.getEngineByName("Foo"),
    null,
    'Engine "Foo" should not be present when test starts'
  );

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "Foo",
            URLTemplate: "http://example.com/?q={searchTerms}",
            IconURL: iconURL,
          },
        ],
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  // If this passes, it means that the new search engine was properly installed

  let engine = Services.search.getEngineByName("Foo");
  isnot(engine, null, "Specified search engine should be installed");

  is(engine.wrappedJSObject.iconURI.spec, iconURL, "Icon should be present");
  is(engine.wrappedJSObject.queryCharset, "UTF-8", "Should default to utf-8");

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Remove: ["Foo"],
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  // If this passes, it means that the specified engine was properly removed
  is(
    Services.search.getEngineByName("Foo"),
    null,
    "Specified search engine should not be installed"
  );

  EnterprisePolicyTesting.resetRunOnceState();
});

add_task(async function test_install_post_method_engine() {
  is(
    Services.search.getEngineByName("Post"),
    null,
    'Engine "Post" should not be present when test starts'
  );

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "Post",
            Method: "POST",
            PostData: "q={searchTerms}&anotherParam=yes",
            URLTemplate: "http://example.com/",
          },
        ],
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  let engine = Services.search.getEngineByName("Post");
  isnot(engine, null, "Specified search engine should be installed");

  is(engine.wrappedJSObject._urls[0].method, "POST", "Method should be POST");

  let submission = engine.getSubmission("term", "text/html");
  isnot(submission.postData, null, "Post data should not be null");

  let scriptableInputStream = Cc[
    "@mozilla.org/scriptableinputstream;1"
  ].createInstance(Ci.nsIScriptableInputStream);
  scriptableInputStream.init(submission.postData);
  is(
    scriptableInputStream.read(scriptableInputStream.available()),
    "q=term&anotherParam=yes",
    "Post data should be present"
  );

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Remove: ["Post"],
      },
    },
  });
  EnterprisePolicyTesting.resetRunOnceState();
});

add_task(async function test_install_with_encoding() {
  // Make sure we are starting in an expected state to avoid false positive
  // test results.
  is(
    Services.search.getEngineByName("Encoding"),
    null,
    'Engine "Encoding" should not be present when test starts'
  );

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "Encoding",
            Encoding: "windows-1252",
            URLTemplate: "http://example.com/?q={searchTerms}",
          },
        ],
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  let engine = Services.search.getEngineByName("Encoding");
  is(
    engine.wrappedJSObject.queryCharset,
    "windows-1252",
    "Should have correct encoding"
  );

  // Clean up
  await Services.search.removeEngine(engine);
  EnterprisePolicyTesting.resetRunOnceState();
});

add_task(async function test_install_and_update() {
  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "ToUpdate",
            URLTemplate: "http://initial.example.com/?q={searchTerms}",
          },
        ],
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  let engine = Services.search.getEngineByName("ToUpdate");
  isnot(engine, null, "Specified search engine should be installed");

  is(
    engine.getSubmission("test").uri.spec,
    "http://initial.example.com/?q=test",
    "Initial submission URL should be correct."
  );

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "ToUpdate",
            URLTemplate: "http://update.example.com/?q={searchTerms}",
          },
        ],
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  is(
    engine.getSubmission("test").uri.spec,
    "http://update.example.com/?q=test",
    "Updated Submission URL should be correct."
  );

  // Clean up
  await Services.search.removeEngine(engine);
  EnterprisePolicyTesting.resetRunOnceState();
});

add_task(async function test_install_with_suggest() {
  // Make sure we are starting in an expected state to avoid false positive
  // test results.
  is(
    Services.search.getEngineByName("Suggest"),
    null,
    'Engine "Suggest" should not be present when test starts'
  );

  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "Suggest",
            URLTemplate: "http://example.com/?q={searchTerms}",
            SuggestURLTemplate: "http://suggest.example.com/?q={searchTerms}",
          },
        ],
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  let engine = Services.search.getEngineByName("Suggest");

  is(
    engine.getSubmission("test", "application/x-suggestions+json").uri.spec,
    "http://suggest.example.com/?q=test",
    "Updated Submission URL should be correct."
  );

  // Clean up
  await Services.search.removeEngine(engine);
  EnterprisePolicyTesting.resetRunOnceState();
});

add_task(async function test_reset_default() {
  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Remove: ["DuckDuckGo"],
      },
    },
  });
  // Get in line, because the Search policy callbacks are async.
  await TestUtils.waitForTick();

  let engine = Services.search.getEngineByName("DuckDuckGo");

  is(engine.hidden, true, "Application specified engine should be hidden.");

  await BrowserTestUtils.withNewTab(
    "about:preferences#search",
    async browser => {
      let tree = browser.contentDocument.querySelector("#engineList");
      for (let i = 0; i < tree.view.rowCount; i++) {
        let cellName = tree.view.getCellText(
          i,
          tree.columns.getNamedColumn("engineName")
        );
        isnot(cellName, "DuckDuckGo", "DuckDuckGo should be invisible");
      }
      let restoreDefaultsButton = browser.contentDocument.getElementById(
        "restoreDefaultSearchEngines"
      );
      let updatedPromise = SearchTestUtils.promiseSearchNotification(
        SearchUtils.MODIFIED_TYPE.CHANGED,
        SearchUtils.TOPIC_ENGINE_MODIFIED
      );
      restoreDefaultsButton.click();
      await updatedPromise;
      for (let i = 0; i < tree.view.rowCount; i++) {
        let cellName = tree.view.getCellText(
          i,
          tree.columns.getNamedColumn("engineName")
        );
        isnot(cellName, "DuckDuckGo", "DuckDuckGo should be invisible");
      }
    }
  );

  engine.hidden = false;
  EnterprisePolicyTesting.resetRunOnceState();
});
