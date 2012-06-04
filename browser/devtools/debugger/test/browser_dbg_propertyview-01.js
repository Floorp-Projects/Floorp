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

    is(ss._trimUrlQuery("a/b/c.d?test=1&random=4"), "a/b/c.d",
      "Trimming the url query isn't done properly.");

    let urls = [
      { href: "ichi://some.address.com/random/", leaf: "subrandom/" },
      { href: "ni://another.address.org/random/subrandom/", leaf: "page.html" },
      { href: "san://interesting.address.gro/random/", leaf: "script.js" },
      { href: "shi://interesting.address.moc/random/", leaf: "script.js" },
      { href: "shi://interesting.address.moc/random/", leaf: "x/script.js" },
      { href: "shi://interesting.address.moc/random/", leaf: "x/y/script.js?a=1" },
      { href: "shi://interesting.address.moc/random/x/", leaf: "y/script.js?a=1&b=2", dupe: true },
      { href: "shi://interesting.address.moc/random/x/y/", leaf: "script.js?a=1&b=2&c=3", dupe: true },
      { href: "go://random/", leaf: "script_t1.js&a=1&b=2&c=3" },
      { href: "roku://random/", leaf: "script_t2.js#id" },
      { href: "nana://random/", leaf: "script_t3.js#id?a=1&b=2" }
    ];

    urls.forEach(function(url) {
      executeSoon(function() {
        let loc = url.href + url.leaf;
        vs.addScript(ss._getScriptLabel(loc, url.href), { url: loc }, true);
      });
    });

    executeSoon(function() {
      urls.forEach(function(url) {
        let loc = url.href + url.leaf;
        ok(url.dupe || vs.contains(loc), "Script url is incorrect: " + loc);
      });

      ok(gDebugger.DebuggerView.Scripts.containsLabel("subrandom/"),
        "Script (0) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("page.html"),
        "Script (1) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("script.js"),
        "Script (2) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("shi://interesting.address.moc/random/script.js"),
        "Script (3) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("x/script.js"),
        "Script (4) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("x/y/script.js"),
        "Script (5) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("script_t1.js"),
        "Script (6) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("script_t2.js"),
        "Script (7) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("script_t3.js"),
        "Script (8) label is incorrect.");

      is(vs._scripts.itemCount, 9,
        "Didn't get the correct number of scripts in the list.");

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
