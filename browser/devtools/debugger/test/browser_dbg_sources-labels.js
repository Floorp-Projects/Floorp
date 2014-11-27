/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that urls are correctly shortened to unique labels.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gUtils;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gUtils = gDebugger.SourceUtils;

    let ellipsis = gPanel.panelWin.L10N.ellipsis;
    let nananana = new Array(20).join(NaN);

    // Test trimming url queries.

    let someUrl = "a/b/c.d?test=1&random=4#reference";
    let shortenedUrl = "a/b/c.d";
    is(gUtils.trimUrlQuery(someUrl), shortenedUrl,
      "Trimming the url query isn't done properly.");

    // Test trimming long urls with an ellipsis.

    let largeLabel = new Array(100).join("Beer can in Jamaican sounds like Bacon!");
    let trimmedLargeLabel = gUtils.trimUrlLength(largeLabel, 1234);
    is(trimmedLargeLabel.length, 1235,
      "Trimming large labels isn't done properly.");
    ok(trimmedLargeLabel.endsWith(ellipsis),
      "Trimming large labels should add an ellipsis at the end.");

    // Test the sources list behaviour with certain urls.

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
    is(gSources.selectedItem.attachment.label, "doc_recursion-stack.html",
      "The first item in the sources widget should be selected (2).");
    is(getSelectedSourceURL(gSources), TAB_URL,
      "The first item in the sources widget should be selected (3).");

    let id = 0;
    for (let { href, leaf } of urls) {
      let url = href + leaf;
      let actor = 'actor' + id++;
      let label = gUtils.trimUrlLength(gUtils.getSourceLabel(url));
      let group = gUtils.getSourceGroup(url);
      let dummy = document.createElement("label");
      dummy.setAttribute('value', label);

      gSources.push([dummy, actor], {
        attachment: {
          source: { actor: actor, url: url },
          label: label,
          group: group
        }
      });
    }

    info("Source locations:");
    info(gSources.values.toSource());

    info("Source attachments:");
    info(gSources.attachments.toSource());

    for (let { href, leaf, dupe } of urls) {
      let url = href + leaf;
      if (dupe) {
        ok(!gSources.containsValue(getSourceActor(gSources, url)), "Shouldn't contain source: " + url);
      } else {
        ok(gSources.containsValue(getSourceActor(gSources, url)), "Should contain source: " + url);
      }
    }

    ok(gSources.getItemForAttachment(e => e.label == "random/subrandom/"),
      "Source (0) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "random/suprandom/?a=1"),
      "Source (1) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "random/?a=1"),
      "Source (2) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "page.html"),
      "Source (3) label is incorrect.");

    ok(gSources.getItemForAttachment(e => e.label == "script.js"),
      "Source (4) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "random/script.js"),
      "Source (5) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "random/x/script.js"),
      "Source (6) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "script.js?a=1"),
      "Source (7) label is incorrect.");

    ok(gSources.getItemForAttachment(e => e.label == "script_t1.js"),
      "Source (8) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "script_t2_1.js"),
      "Source (9) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "script_t2_2.js"),
      "Source (10) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "script_t2_3.js"),
      "Source (11) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "script_t3_1.js"),
      "Source (12) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "script_t3_2.js"),
      "Source (13) label is incorrect.");
    ok(gSources.getItemForAttachment(e => e.label == "script_t3_3.js"),
      "Source (14) label is incorrect.");

    ok(gSources.getItemForAttachment(e => e.label == nananana + "Batman!" + ellipsis),
      "Source (15) label is incorrect.");

    is(gSources.itemCount, urls.filter(({ dupe }) => !dupe).length + 1,
      "Didn't get the correct number of sources in the list.");

    is(gSources.getItemByValue(getSourceActor(gSources, "http://some.address.com/random/subrandom/")).attachment.label,
      "random/subrandom/",
      "gSources.getItemByValue isn't functioning properly (0).");
    is(gSources.getItemByValue(getSourceActor(gSources, "http://some.address.com/random/suprandom/?a=1")).attachment.label,
      "random/suprandom/?a=1",
      "gSources.getItemByValue isn't functioning properly (1).");

    is(gSources.getItemForAttachment(e => e.label == "random/subrandom/").attachment.source.url,
       "http://some.address.com/random/subrandom/",
      "gSources.getItemForAttachment isn't functioning properly (0).");
    is(gSources.getItemForAttachment(e => e.label == "random/suprandom/?a=1").attachment.source.url,
      "http://some.address.com/random/suprandom/?a=1",
      "gSources.getItemForAttachment isn't functioning properly (1).");

    closeDebuggerAndFinish(gPanel);
  });
}
