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

      let globalScope = gDebugger.DebuggerView.Properties._globalScope;
      let localScope = gDebugger.DebuggerView.Properties._localScope;
      let withScope = gDebugger.DebuggerView.Properties._withScope;
      let closureScope = gDebugger.DebuggerView.Properties._closureScope;

      ok(globalScope,
        "Should have a globalScope container for the variables property view.");

      ok(localScope,
        "Should have a localScope container for the variables property view.");

      ok(withScope,
        "Should have a withScope container for the variables property view.");

      ok(closureScope,
        "Should have a closureScope container for the variables property view.");

      is(globalScope, gDebugger.DebuggerView.Properties.globalScope,
        "The globalScope object should be equal to the corresponding getter.");

      is(localScope, gDebugger.DebuggerView.Properties.localScope,
        "The localScope object should be equal to the corresponding getter.");

      is(withScope, gDebugger.DebuggerView.Properties.withScope,
        "The withScope object should be equal to the corresponding getter.");

      is(closureScope, gDebugger.DebuggerView.Properties.closureScope,
        "The closureScope object should be equal to the corresponding getter.");


      ok(!globalScope.expanded,
        "The globalScope should be initially collapsed.");

      ok(localScope.expanded,
        "The localScope should be initially expanded.");

      ok(!withScope.expanded,
        "The withScope should be initially collapsed.");

      ok(!withScope.visible,
        "The withScope should be initially hidden.");

      ok(!closureScope.expanded,
        "The closureScope should be initially collapsed.");

      ok(!closureScope.visible,
        "The closureScope should be initially hidden.");

      is(gDebugger.DebuggerView.Properties._vars.childNodes.length, 4,
        "Should have only 4 scopes created: global, local, with and scope.");

      resumeAndFinish();
    }}, 0);
  });

  gDebuggee.simpleCall();
}

function resumeAndFinish() {
  gDebugger.DebuggerController.activeThread.resume(function() {
    let vs = gDebugger.DebuggerView.Scripts;
    let ss = gDebugger.DebuggerController.SourceScripts;
    vs.empty();
    vs._scripts.removeEventListener("select", vs._onScriptsChange, false);

    is(ss._trimUrlQuery("a/b/c.d?test=1&random=4"), "a/b/c.d",
      "Trimming the url query isn't done properly.");

    let urls = [
      { href: "ici://some.address.com/random/", leaf: "subrandom/" },
      { href: "ni://another.address.org/random/subrandom/", leaf: "page.html" },
      { href: "san://interesting.address.gro/random/", leaf: "script.js" },
      { href: "si://interesting.address.moc/random/", leaf: "script.js" },
      { href: "si://interesting.address.moc/random/", leaf: "x/script.js" },
      { href: "si://interesting.address.moc/random/", leaf: "x/y/script.js?a=1" },
      { href: "si://interesting.address.moc/random/x/", leaf: "y/script.js?a=1&b=2" },
      { href: "si://interesting.address.moc/random/x/y/", leaf: "script.js?a=1&b=2&c=3" }
    ];

    urls.forEach(function(url) {
      executeSoon(function() {
        let loc = url.href + url.leaf;
        vs.addScript(ss._getScriptLabel(loc, url.href), { url: loc });
        vs.commitScripts();
      });
    });

    executeSoon(function() {
      for (let i = 0; i < vs._scripts.itemCount; i++) {
        let lab = vs._scripts.getItemAtIndex(i).getAttribute("label");
        let loc = urls[i].href + urls[i].leaf;

        info("label: " + i + " " + lab);
        ok(vs.contains(loc), "Script url is incorrect: " + loc);
      }

      ok(gDebugger.DebuggerView.Scripts.containsLabel("subrandom/"),
        "Script (0) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("page.html"),
        "Script (1) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("script.js"),
        "Script (2) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("si://interesting.address.moc/random/script.js"),
        "Script (3) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("x/script.js"),
        "Script (4) label is incorrect.");
      ok(gDebugger.DebuggerView.Scripts.containsLabel("x/y/script.js"),
        "Script (5) label is incorrect.");

      is(vs._scripts.itemCount, 6,
        "Got too many script items in the list!");

      closeDebuggerAndFinish(gTab);
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
