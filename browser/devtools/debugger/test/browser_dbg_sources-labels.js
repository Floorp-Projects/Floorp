/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that urls are correctly shortened to unique labels.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gSources, gUtils;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gUtils = gDebugger.SourceUtils;

    let ellipsis = gPanel.panelWin.L10N.ellipsis;
    let nananana = new Array(20).join(NaN);

    let someUrl = "a/b/c.d?test=1&random=4#reference";
    let shortenedUrl = "a/b/c.d";
    is(gUtils.trimUrlQuery(someUrl), shortenedUrl,
      "Trimming the url query isn't done properly.");

    let largeLabel = new Array(100).join("Beer can in Jamaican sounds like Bacon!");
    let trimmedLargeLabel = gUtils.trimUrlLength(largeLabel, 1234);
    is(trimmedLargeLabel.length, 1235,
      "Trimming large labels isn't done properly.");
    ok(trimmedLargeLabel.endsWith(ellipsis),
      "Trimming large labels should add an ellipsis at the end.");

    let urls = [
      { href: "http://some.address.com/random/", leaf: "subrandom/" },
      { href: "http://some.address.com/random/", leaf: "suprandom/?a=1" },
      { href: "http://some.address.com/random/", leaf: "?a=1" },
      { href: "https://another.address.org/random/subrandom/", leaf: "page.html" },

      { href: "ftp://interesting.address.org/random/", leaf: "script.js" },
      { href: "ftp://interesting.address.com/random/", leaf: "script.js" },
      { href: "ftp://interesting.address.com/random/", leaf: "x/script.js" },
      { href: "ftp://interesting.address.com/random/", leaf: "x/y/script.js?a=1" },
      { href: "ftp://interesting.address.com/random/x/", leaf: "y/script.js?a=1&b=2" },
      { href: "ftp://interesting.address.com/random/x/y/", leaf: "script.js?a=1&b=2&c=3" },
      { href: "ftp://interesting.address.com/random/", leaf: "x/y/script.js?a=2" },
      { href: "ftp://interesting.address.com/random/x/", leaf: "y/script.js?a=2&b=3" },
      { href: "ftp://interesting.address.com/random/x/y/", leaf: "script.js?a=2&b=3&c=4" },

      { href: "file://random/", leaf: "script_t1.js&a=1&b=2&c=3" },
      { href: "file://random/", leaf: "script_t2_1.js#id" },
      { href: "file://random/", leaf: "script_t2_2.js?a" },
      { href: "file://random/", leaf: "script_t2_3.js&b" },
      { href: "resource://random/", leaf: "script_t3_1.js#id?a=1&b=2" },
      { href: "resource://random/", leaf: "script_t3_2.js?a=1&b=2#id" },
      { href: "resource://random/", leaf: "script_t3_3.js&a=1&b=2#id" },

      { href: nananana, leaf: "Batman!" + "{trim me, now and forevermore}" }
    ];

    is(gSources.itemCount, 1,
      "Should contain the original source label in the sources widget.");
    is(gSources.selectedIndex, 0,
      "The first item in the sources widget should be selected (1).");
    is(gSources.selectedLabel, "doc_recursion-stack.html",
      "The first item in the sources widget should be selected (2).");
    is(gSources.selectedValue, TAB_URL,
      "The first item in the sources widget should be selected (3).");

    gSources.empty();

    is(gSources.itemCount, 0,
      "Should contain no items in the sources widget after emptying.");
    is(gSources.selectedIndex, -1,
      "No item in the sources widget should be selected (1).");
    is(gSources.selectedLabel, "",
      "No item in the sources widget should be selected (2).");
    is(gSources.selectedValue, "",
      "No item in the sources widget should be selected (3).");

    for (let { href, leaf } of urls) {
      let url = href + leaf;
      let label = gUtils.getSourceLabel(url);
      let trimmedLabel = gUtils.trimUrlLength(label);
      gSources.push([trimmedLabel, url], { attachment: {}});
    }

    info("Source labels:");
    info(gSources.labels.toSource());

    info("Source locations:");
    info(gSources.values.toSource());

    for (let { href, leaf, dupe } of urls) {
      let url = href + leaf;
      if (dupe) {
        ok(!gSources.containsValue(url), "Shouldn't contain source: " + url);
      } else {
        ok(gSources.containsValue(url), "Should contain source: " + url);
      }
    }

    ok(gSources.containsLabel("random/subrandom/"),
      "Source (0) label is incorrect.");
    ok(gSources.containsLabel("random/suprandom/?a=1"),
      "Source (1) label is incorrect.");
    ok(gSources.containsLabel("random/?a=1"),
      "Source (2) label is incorrect.");
    ok(gSources.containsLabel("page.html"),
      "Source (3) label is incorrect.");

    ok(gSources.containsLabel("script.js"),
      "Source (4) label is incorrect.");
    ok(gSources.containsLabel("random/script.js"),
      "Source (5) label is incorrect.");
    ok(gSources.containsLabel("random/x/script.js"),
      "Source (6) label is incorrect.");
    ok(gSources.containsLabel("script.js?a=1"),
      "Source (7) label is incorrect.");

    ok(gSources.containsLabel("script_t1.js"),
      "Source (8) label is incorrect.");
    ok(gSources.containsLabel("script_t2_1.js"),
      "Source (9) label is incorrect.");
    ok(gSources.containsLabel("script_t2_2.js"),
      "Source (10) label is incorrect.");
    ok(gSources.containsLabel("script_t2_3.js"),
      "Source (11) label is incorrect.");
    ok(gSources.containsLabel("script_t3_1.js"),
      "Source (12) label is incorrect.");
    ok(gSources.containsLabel("script_t3_2.js"),
      "Source (13) label is incorrect.");
    ok(gSources.containsLabel("script_t3_3.js"),
      "Source (14) label is incorrect.");

    ok(gSources.containsLabel(nananana + "Batman!" + ellipsis),
      "Source (15) label is incorrect.");

    is(gSources.itemCount, urls.filter(({ dupe }) => !dupe).length,
      "Didn't get the correct number of sources in the list.");

    is(gSources.getItemByValue("http://some.address.com/random/subrandom/").label,
      "random/subrandom/",
      "gSources.getItemByValue isn't functioning properly (0).");
    is(gSources.getItemByValue("http://some.address.com/random/suprandom/?a=1").label,
      "random/suprandom/?a=1",
      "gSources.getItemByValue isn't functioning properly (1).");

    is(gSources.getItemByLabel("random/subrandom/").value,
      "http://some.address.com/random/subrandom/",
      "gSources.getItemByLabel isn't functioning properly (0).");
    is(gSources.getItemByLabel("random/suprandom/?a=1").value,
      "http://some.address.com/random/suprandom/?a=1",
      "gSources.getItemByLabel isn't functioning properly (1).");

    closeDebuggerAndFinish(gPanel);
  });
}
