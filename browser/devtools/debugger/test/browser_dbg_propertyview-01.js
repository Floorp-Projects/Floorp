/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test() {
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.contentWindow;

    testSimpleCall();
  });
}

function testSimpleCall() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      testScriptLabelShortening();
    }}, 0);
  });

  gDebuggee.simpleCall();
}

function testScriptLabelShortening() {
  gDebugger.DebuggerController.activeThread.resume(function() {
    let vs = gDebugger.DebuggerView.Scripts;
    let ss = gDebugger.DebuggerController.SourceScripts;
    vs.empty();
    vs._scripts.removeEventListener("select", vs._onScriptsChange, false);

    is(ss.trimUrlQuery("a/b/c.d?test=1&random=4#reference"), "a/b/c.d",
      "Trimming the url query isn't done properly.");

    let ellipsis = Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString);
    let nanana = new Array(20).join(NaN);

    let superLargeLabel = new Array(100).join("Beer can in Jamaican sounds like Bacon!");
    let trimmedLargeLabel = ss.trimUrlLength(superLargeLabel, 1234);
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
      { href: "ftp://interesting.address.com/random/x/", leaf: "y/script.js?a=1&b=2", dupe: true },
      { href: "ftp://interesting.address.com/random/x/y/", leaf: "script.js?a=1&b=2&c=3", dupe: true },
      { href: "ftp://interesting.address.com/random/", leaf: "x/y/script.js?a=2", dupe: true },
      { href: "ftp://interesting.address.com/random/x/", leaf: "y/script.js?a=2&b=3", dupe: true },
      { href: "ftp://interesting.address.com/random/x/y/", leaf: "script.js?a=2&b=3&c=4", dupe: true },

      { href: "file://random/", leaf: "script_t1.js&a=1&b=2&c=3" },
      { href: "file://random/", leaf: "script_t2_1.js#id" },
      { href: "file://random/", leaf: "script_t2_2.js?a" },
      { href: "file://random/", leaf: "script_t2_3.js&b" },
      { href: "resource://random/", leaf: "script_t3_1.js#id?a=1&b=2" },
      { href: "resource://random/", leaf: "script_t3_2.js?a=1&b=2#id" },
      { href: "resource://random/", leaf: "script_t3_3.js&a=1&b=2#id" },

      { href: nanana, leaf: "Batman!" + "{trim me, now and forevermore}" }
    ];

    urls.forEach(function(url) {
      executeSoon(function() {
        let loc = url.href + url.leaf;
        vs.addScript(ss.getScriptLabel(loc, url.href), { url: loc }, true);
      });
    });

    executeSoon(function() {
      info("Script labels:");
      info(vs.scriptLabels.toSource());

      info("Script locations:");
      info(vs.scriptLocations.toSource());

      urls.forEach(function(url) {
        let loc = url.href + url.leaf;
        if (url.dupe) {
          ok(!vs.contains(loc), "Shouldn't contain script: " + loc);
        } else {
          ok(vs.contains(loc), "Should contain script: " + loc);
        }
      });

      ok(vs.containsLabel("random/subrandom/"),
        "Script (0) label is incorrect.");
      ok(vs.containsLabel("random/suprandom/?a=1"),
        "Script (1) label is incorrect.");
      ok(vs.containsLabel("random/?a=1"),
        "Script (2) label is incorrect.");
      ok(vs.containsLabel("page.html"),
        "Script (3) label is incorrect.");

      ok(vs.containsLabel("script.js"),
        "Script (4) label is incorrect.");
      ok(vs.containsLabel("random/script.js"),
        "Script (5) label is incorrect.");
      ok(vs.containsLabel("random/x/script.js"),
        "Script (6) label is incorrect.");
      ok(vs.containsLabel("script.js?a=1"),
        "Script (7) label is incorrect.");

      ok(vs.containsLabel("script_t1.js"),
        "Script (8) label is incorrect.");
      ok(vs.containsLabel("script_t2_1.js"),
        "Script (9) label is incorrect.");
      ok(vs.containsLabel("script_t2_2.js"),
        "Script (10) label is incorrect.");
      ok(vs.containsLabel("script_t2_3.js"),
        "Script (11) label is incorrect.");
      ok(vs.containsLabel("script_t3_1.js"),
        "Script (12) label is incorrect.");
      ok(vs.containsLabel("script_t3_2.js"),
        "Script (13) label is incorrect.");
      ok(vs.containsLabel("script_t3_3.js"),
        "Script (14) label is incorrect.");

      ok(vs.containsLabel(nanana + "Batman!" + ellipsis),
        "Script (15) label is incorrect.");

      is(vs._scripts.itemCount, urls.filter(function(url) !url.dupe).length,
        "Didn't get the correct number of scripts in the list.");

      is(vs.getScriptByLocation("http://some.address.com/random/subrandom/").label, "random/subrandom/",
        "Scripts.getScriptByLocation isn't functioning properly (0).");
      is(vs.getScriptByLocation("http://some.address.com/random/suprandom/?a=1").label, "random/suprandom/?a=1",
        "Scripts.getScriptByLocation isn't functioning properly (1).");

      is(vs.getScriptByLabel("random/subrandom/").value, "http://some.address.com/random/subrandom/",
        "Scripts.getScriptByLabel isn't functioning properly (0).");
      is(vs.getScriptByLabel("random/suprandom/?a=1").value, "http://some.address.com/random/suprandom/?a=1",
        "Scripts.getScriptByLabel isn't functioning properly (1).");


      closeDebuggerAndFinish();
    });
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
