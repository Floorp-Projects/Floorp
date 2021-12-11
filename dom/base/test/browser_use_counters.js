/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */

requestLongerTimeout(2);

const gHttpTestRoot = "https://example.com/browser/dom/base/test/";

/**
 * Enable local telemetry recording for the duration of the tests.
 */
var gOldContentCanRecord = false;
var gOldParentCanRecord = false;
add_task(async function test_initialize() {
  let Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(
    Ci.nsITelemetry
  );
  gOldParentCanRecord = Telemetry.canRecordExtended;
  Telemetry.canRecordExtended = true;

  await SpecialPowers.pushPrefEnv({
    set: [
      // Because canRecordExtended is a per-process variable, we need to make sure
      // that all of the pages load in the same content process. Limit the number
      // of content processes to at most 1 (or 0 if e10s is off entirely).
      ["dom.ipc.processCount", 1],
      ["layout.css.use-counters.enabled", true],
      ["layout.css.use-counters-unimplemented.enabled", true],
    ],
  });

  gOldContentCanRecord = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      let telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(
        Ci.nsITelemetry
      );
      let old = telemetry.canRecordExtended;
      telemetry.canRecordExtended = true;
      return old;
    }
  );
  info("canRecord for content: " + gOldContentCanRecord);
});

add_task(async function() {
  const TESTS = [
    // Check that use counters are incremented by SVGs loaded directly in iframes.
    {
      type: "iframe",
      filename: "file_use_counter_svg_getElementById.svg",
      counters: [{ name: "SVGSVGELEMENT_GETELEMENTBYID" }],
    },
    {
      type: "iframe",
      filename: "file_use_counter_svg_currentScale.svg",
      counters: [
        { name: "SVGSVGELEMENT_CURRENTSCALE_getter" },
        { name: "SVGSVGELEMENT_CURRENTSCALE_setter" },
      ],
    },

    {
      type: "iframe",
      filename: "file_use_counter_style.html",
      counters: [
        // Check for longhands.
        { name: "CSS_PROPERTY_BackgroundImage" },
        // Check for shorthands.
        { name: "CSS_PROPERTY_Padding" },
        // Check for aliases.
        { name: "CSS_PROPERTY_MozTransform" },
        // Check for counted unknown properties.
        { name: "CSS_PROPERTY_WebkitPaddingStart" },
      ],
    },

    // Check that even loads from the imglib cache update use counters.  The
    // images should still be there, because we just loaded them in the last
    // set of tests.  But we won't get updated counts for the document
    // counters, because we won't be re-parsing the SVG documents.
    {
      type: "iframe",
      filename: "file_use_counter_svg_getElementById.svg",
      counters: [{ name: "SVGSVGELEMENT_GETELEMENTBYID" }],
      check_documents: false,
    },
    {
      type: "iframe",
      filename: "file_use_counter_svg_currentScale.svg",
      counters: [
        { name: "SVGSVGELEMENT_CURRENTSCALE_getter" },
        { name: "SVGSVGELEMENT_CURRENTSCALE_setter" },
      ],
      check_documents: false,
    },

    // Check that use counters are incremented by SVGs loaded as images.
    // Note that SVG images are not permitted to execute script, so we can only
    // check for properties here.
    {
      type: "img",
      filename: "file_use_counter_svg_getElementById.svg",
      counters: [{ name: "CSS_PROPERTY_Fill" }],
    },
    {
      type: "img",
      filename: "file_use_counter_svg_currentScale.svg",
      counters: [{ name: "CSS_PROPERTY_Fill" }],
    },

    // Check that use counters are incremented by directly loading SVGs
    // that reference patterns defined in another SVG file.
    {
      type: "direct",
      filename: "file_use_counter_svg_fill_pattern.svg",
      counters: [{ name: "CSS_PROPERTY_FillOpacity", xfail: true }],
    },

    // Check that use counters are incremented by directly loading SVGs
    // that reference patterns defined in the same file or in data: URLs.
    {
      type: "direct",
      filename: "file_use_counter_svg_fill_pattern_internal.svg",
      counters: [{ name: "CSS_PROPERTY_FillOpacity" }],
    },

    // Check that use counters are incremented in a display:none iframe.
    {
      type: "undisplayed-iframe",
      filename: "file_use_counter_svg_currentScale.svg",
      counters: [{ name: "SVGSVGELEMENT_CURRENTSCALE_getter" }],
    },

    // Check that a document that comes out of the bfcache reports any new use
    // counters recorded on it.
    {
      type: "direct",
      filename: "file_use_counter_bfcache.html",
      waitForExplicitFinish: true,
      counters: [{ name: "SVGSVGELEMENT_GETELEMENTBYID" }],
    },

    // // data: URLs don't correctly propagate to their referring document yet.
    // {
    //   type: "direct",
    //   filename: "file_use_counter_svg_fill_pattern_data.svg",
    //   counters: [
    //     { name: "PROPERTY_FILL_OPACITY" },
    //   ],
    // },
  ];

  for (let test of TESTS) {
    let file = test.filename;
    info(`checking ${file} (${test.type})`);

    let newTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
    gBrowser.selectedTab = newTab;
    newTab.linkedBrowser.stop();

    // Hold on to the current values of the telemetry histograms we're
    // interested in.
    let before = await grabHistogramsFromContent(
      test.counters.map(c => c.name)
    );

    // Load the test file in the new tab, either directly or via
    // file_use_counter_outer{,_display_none}.html, depending on the test type.
    let url, targetElement;
    switch (test.type) {
      case "iframe":
        url = gHttpTestRoot + "file_use_counter_outer.html";
        targetElement = "content";
        break;
      case "undisplayed-iframe":
        url = gHttpTestRoot + "file_use_counter_outer_display_none.html";
        targetElement = "content";
        break;
      case "img":
        url = gHttpTestRoot + "file_use_counter_outer.html";
        targetElement = "display";
        break;
      case "direct":
        url = gHttpTestRoot + file;
        targetElement = null;
        break;
      default:
        throw `unexpected type ${test.type}`;
    }

    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    if (test.waitForExplicitFinish) {
      if (test.type != "direct") {
        throw new Error(
          `cannot use waitForExplicitFinish with test type ${test.type}`
        );
      }

      // Wait until the tab changes its hash to indicate it has finished.
      await BrowserTestUtils.waitForLocationChange(gBrowser, url + "#finished");
    }

    if (targetElement) {
      // Inject our desired file into the target element of the newly-loaded page.
      await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [{ file, targetElement }],
        function(opts) {
          let target = content.document.getElementById(opts.targetElement);
          target.src = opts.file;

          return new Promise(resolve => {
            let listener = event => {
              event.target.removeEventListener("load", listener, true);
              resolve();
            };
            target.addEventListener("load", listener, true);
          });
        }
      );
    }

    // Tear down the page.
    let tabClosed = BrowserTestUtils.waitForTabClosing(newTab);
    gBrowser.removeTab(newTab);
    await tabClosed;

    // Grab histograms again.
    let after = await grabHistogramsFromContent(
      test.counters.map(c => c.name),
      before.sentinel
    );

    // Compare before and after.
    for (let counter of test.counters) {
      let name = counter.name;
      let value = counter.value ?? 1;
      if (!counter.xfail) {
        is(
          after.page[name],
          before.page[name] + value,
          `page counts for ${name} after are correct`
        );
        is(
          after.document[name],
          before.document[name] + value,
          `document counts for ${name} after are correct`
        );
      }
    }

    if (test.check_documents ?? true) {
      ok(
        after.toplevel_docs >= before.toplevel_docs + 1,
        "top level destroyed document counts are correct"
      );
      // 2 documents for "img" tests: one for the outer html page containing the
      // <img> element, and one for the SVG image itself.
      ok(
        after.docs >= before.docs + (test.type == "img" ? 2 : 1),
        "destroyed document counts are correct"
      );
    }
  }
});

add_task(async function() {
  let Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(
    Ci.nsITelemetry
  );
  Telemetry.canRecordExtended = gOldParentCanRecord;

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ oldCanRecord: gOldContentCanRecord }],
    async function(arg) {
      await new Promise(resolve => {
        let telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(
          Ci.nsITelemetry
        );
        telemetry.canRecordExtended = arg.oldCanRecord;
        resolve();
      });
    }
  );
});

async function grabHistogramsFromContent(names, prev_sentinel = null) {
  // We don't have any way to get a notification when telemetry from the
  // document that was just closed has been reported. So instead, we
  // repeatedly poll for telemetry until we see that a specific use counter
  // histogram (CSS_PROPERTY_MarkerMid, the "sentinel") that likely is not
  // used by any other document that's open has been incremented.
  let telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(
    Ci.nsITelemetry
  );
  let gatheredHistograms;
  return BrowserTestUtils.waitForCondition(
    function() {
      // Document use counters are reported in the content process (when e10s
      // is enabled), and page use counters are reported in the parent process.
      let snapshots = telemetry.getSnapshotForHistograms("main", false);
      let checkGet = probe => {
        // When e10s is disabled, all histograms are reported in the parent
        // process.  Otherwise, all page use counters are reported in the parent
        // and document use counters are reported in the content process.
        let process =
          !Services.appinfo.browserTabsRemoteAutostart ||
          probe.endsWith("_PAGE") ||
          probe == "TOP_LEVEL_CONTENT_DOCUMENTS_DESTROYED"
            ? "parent"
            : "content";
        return snapshots[process][probe] ? snapshots[process][probe].sum : 0;
      };
      let page = Object.fromEntries(
        names.map(name => [name, checkGet(`USE_COUNTER2_${name}_PAGE`)])
      );
      let document = Object.fromEntries(
        names.map(name => [name, checkGet(`USE_COUNTER2_${name}_DOCUMENT`)])
      );
      gatheredHistograms = {
        page,
        document,
        docs: checkGet("CONTENT_DOCUMENTS_DESTROYED"),
        toplevel_docs: checkGet("TOP_LEVEL_CONTENT_DOCUMENTS_DESTROYED"),
        sentinel: {
          doc: checkGet("USE_COUNTER2_CSS_PROPERTY_MarkerMid_DOCUMENT"),
          page: checkGet("USE_COUNTER2_CSS_PROPERTY_MarkerMid_PAGE"),
        },
      };
      let sentinelChanged =
        !prev_sentinel ||
        (prev_sentinel.doc != gatheredHistograms.sentinel.doc &&
          prev_sentinel.page != gatheredHistograms.sentinel.page);
      return sentinelChanged;
    },
    "grabHistogramsFromContent",
    100,
    Infinity
  ).then(
    () => gatheredHistograms,
    function(msg) {
      throw msg;
    }
  );
}
