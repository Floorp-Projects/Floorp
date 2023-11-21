/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */

requestLongerTimeout(2);

const gHttpTestRoot = "https://example.com/browser/dom/base/test/";

add_task(async function test_initialize() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["layout.css.use-counters.enabled", true],
      ["layout.css.use-counters-unimplemented.enabled", true],
    ],
  });
});

add_task(async function () {
  const TESTS = [
    // Check that use counters are incremented by SVGs loaded directly in iframes.
    {
      type: "iframe",
      filename: "file_use_counter_svg_getElementById.svg",
      counters: [
        {
          name: "SVGSVGELEMENT_GETELEMENTBYID",
          glean: ["", "svgsvgelementGetelementbyid"],
        },
      ],
    },
    {
      type: "iframe",
      filename: "file_use_counter_svg_currentScale.svg",
      counters: [
        {
          name: "SVGSVGELEMENT_CURRENTSCALE_getter",
          glean: ["", "svgsvgelementCurrentscaleGetter"],
        },
        {
          name: "SVGSVGELEMENT_CURRENTSCALE_setter",
          glean: ["", "svgsvgelementCurrentscaleSetter"],
        },
      ],
    },

    {
      type: "iframe",
      filename: "file_use_counter_style.html",
      counters: [
        // Check for longhands.
        {
          name: "CSS_PROPERTY_BackgroundImage",
          glean: ["Css", "cssBackgroundImage"],
        },
        // Check for shorthands.
        { name: "CSS_PROPERTY_Padding", glean: ["Css", "cssPadding"] },
        // Check for aliases.
        {
          name: "CSS_PROPERTY_MozAppearance",
          glean: ["Css", "cssMozAppearance"],
        },
        // Check for counted unknown properties.
        {
          name: "CSS_PROPERTY_WebkitPaddingStart",
          glean: ["Css", "webkitPaddingStart"],
        },
      ],
    },

    // Check that even loads from the imglib cache update use counters.  The
    // images should still be there, because we just loaded them in the last
    // set of tests.  But we won't get updated counts for the document
    // counters, because we won't be re-parsing the SVG documents.
    {
      type: "iframe",
      filename: "file_use_counter_svg_getElementById.svg",
      counters: [
        {
          name: "SVGSVGELEMENT_GETELEMENTBYID",
          glean: ["", "svgsvgelementGetelementbyid"],
        },
      ],
    },
    {
      type: "iframe",
      filename: "file_use_counter_svg_currentScale.svg",
      counters: [
        {
          name: "SVGSVGELEMENT_CURRENTSCALE_getter",
          glean: ["", "svgsvgelementCurrentscaleGetter"],
        },
        {
          name: "SVGSVGELEMENT_CURRENTSCALE_setter",
          glean: ["", "svgsvgelementCurrentscaleSetter"],
        },
      ],
    },

    // Check that use counters are incremented by SVGs loaded as images.
    // Note that SVG images are not permitted to execute script, so we can only
    // check for properties here.
    {
      type: "img",
      filename: "file_use_counter_svg_getElementById.svg",
      counters: [{ name: "CSS_PROPERTY_Fill", glean: ["Css", "cssFill"] }],
    },
    {
      type: "img",
      filename: "file_use_counter_svg_currentScale.svg",
      counters: [{ name: "CSS_PROPERTY_Fill", glean: ["Css", "cssFill"] }],
    },

    // Check that use counters are incremented by directly loading SVGs
    // that reference patterns defined in another SVG file.
    {
      type: "direct",
      filename: "file_use_counter_svg_fill_pattern.svg",
      counters: [
        {
          name: "CSS_PROPERTY_FillOpacity",
          glean: ["Css", "cssFillOpacity"],
          xfail: true,
        },
      ],
    },

    // Check that use counters are incremented by directly loading SVGs
    // that reference patterns defined in the same file or in data: URLs.
    {
      type: "direct",
      filename: "file_use_counter_svg_fill_pattern_internal.svg",
      counters: [
        { name: "CSS_PROPERTY_FillOpacity", glean: ["Css", "cssFillOpacity"] },
      ],
    },

    // Check that use counters are incremented in a display:none iframe.
    {
      type: "undisplayed-iframe",
      filename: "file_use_counter_svg_currentScale.svg",
      counters: [
        {
          name: "SVGSVGELEMENT_CURRENTSCALE_getter",
          glean: ["", "svgsvgelementCurrentscaleGetter"],
        },
      ],
    },

    // Check that a document that comes out of the bfcache reports any new use
    // counters recorded on it.
    {
      type: "direct",
      filename: "file_use_counter_bfcache.html",
      waitForExplicitFinish: true,
      counters: [
        {
          name: "SVGSVGELEMENT_GETELEMENTBYID",
          glean: ["", "svgsvgelementGetelementbyid"],
        },
      ],
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

    // Hold on to the current values of the telemetry histograms we're
    // interested in. Opening an about:blank tab shouldn't change those.
    let before = await grabHistogramsFromContent(
      test.counters.map(c => c.name)
    );

    await Services.fog.testFlushAllChildren();
    before.gleanPage = Object.fromEntries(
      test.counters.map(c => [
        c.name,
        Glean[`useCounter${c.glean[0]}Page`][c.glean[1]].testGetValue() ?? 0,
      ])
    );
    before.gleanDoc = Object.fromEntries(
      test.counters.map(c => [
        c.name,
        Glean[`useCounter${c.glean[0]}Doc`][c.glean[1]].testGetValue() ?? 0,
      ])
    );
    before.glean_docs_destroyed =
      Glean.useCounter.contentDocumentsDestroyed.testGetValue();
    before.glean_toplevel_destroyed =
      Glean.useCounter.topLevelContentDocumentsDestroyed.testGetValue();

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

    let waitForFinish = null;
    if (test.waitForExplicitFinish) {
      is(
        test.type,
        "direct",
        `cannot use waitForExplicitFinish with test type ${test.type}`
      );
      // Wait until the tab changes its hash to indicate it has finished.
      waitForFinish = BrowserTestUtils.waitForLocationChange(
        gBrowser,
        url + "#finished"
      );
    }

    let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    if (waitForFinish) {
      await waitForFinish;
    }

    if (targetElement) {
      // Inject our desired file into the target element of the newly-loaded page.
      await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [{ file, targetElement }],
        function (opts) {
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
    await BrowserTestUtils.removeTab(newTab);

    // Grab histograms again.
    let after = await grabHistogramsFromContent(
      test.counters.map(c => c.name),
      before.sentinel
    );
    await Services.fog.testFlushAllChildren();
    after.gleanPage = Object.fromEntries(
      test.counters.map(c => [
        c.name,
        Glean[`useCounter${c.glean[0]}Page`][c.glean[1]].testGetValue() ?? 0,
      ])
    );
    after.gleanDoc = Object.fromEntries(
      test.counters.map(c => [
        c.name,
        Glean[`useCounter${c.glean[0]}Doc`][c.glean[1]].testGetValue() ?? 0,
      ])
    );
    after.glean_docs_destroyed =
      Glean.useCounter.contentDocumentsDestroyed.testGetValue();
    after.glean_toplevel_destroyed =
      Glean.useCounter.topLevelContentDocumentsDestroyed.testGetValue();

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
        is(
          after.gleanPage[name],
          before.gleanPage[name] + value,
          `Glean page counts for ${name} are correct`
        );
        is(
          after.gleanDoc[name],
          before.gleanDoc[name] + value,
          `Glean document counts for ${name} are correct`
        );
      }
    }

    if (test.filename == "file_use_counter_bfcache.html") {
      // This test navigates a bunch of times and thus creates multiple top
      // level document entries, as expected.
      // Whether the last document is destroyed is a bit racy, see bug 1842800,
      // so for now we allow it with +/- 1.
      ok(
        after.toplevel_docs == before.toplevel_docs + 5 ||
          after.toplevel_docs == before.toplevel_docs + 6,
        `top level destroyed document counts are correct: ${before.toplevel_docs} vs ${after.toplevel_docs}`
      );
      ok(
        after.glean_toplevel_destroyed == before.glean_toplevel_destroyed + 5 ||
          after.glean_toplevel_destroyed == before.glean_toplevel_destroyed + 6,
        `Glean top level destroyed docs counts are correct: ${before.glean_toplevel_destroyed} vs ${after.glean_toplevel_destroyed}`
      );
    } else {
      is(
        after.toplevel_docs,
        before.toplevel_docs + 1,
        "top level destroyed document counts are correct"
      );
      is(
        after.glean_toplevel_destroyed,
        before.glean_toplevel_destroyed + 1,
        "Glean top level destroyed document counts are correct"
      );
    }

    // 2 documents for "img" tests: one for the outer html page containing the
    // <img> element, and one for the SVG image itself.
    // FIXME: iframe tests and so on probably should get two at least.
    ok(
      after.docs >= before.docs + (test.type == "img" ? 2 : 1),
      "destroyed document counts are correct"
    );
    Assert.greaterOrEqual(
      after.glean_docs_destroyed,
      before.glean_docs_destroyed + (test.type == "img" ? 2 : 1),
      "Glean destroyed doc counts are correct"
    );
  }
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
    function () {
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
    function (msg) {
      throw msg;
    }
  );
}
