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
    gDebugger = gPane.debuggerWindow;

    testSimpleCall();
  });
}

function testSimpleCall() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {
      resumeAndFinish();
    }}, 0);
  });

  gDebuggee.simpleCall();
}

function resumeAndFinish() {
  gDebugger.DebuggerController.activeThread.resume(function() {
    checkScriptsOrder();
    addScriptsAndCheckOrder(1, function() {
      addScriptsAndCheckOrder(2, function() {
        addScriptsAndCheckOrder(3, function() {
          closeDebuggerAndFinish(gTab);
        });
      });
    });
  });
}

function addScriptsAndCheckOrder(method, callback) {
  let vs = gDebugger.DebuggerView.Scripts;
  let ss = gDebugger.DebuggerController.SourceScripts;
  vs.empty();
  vs._scripts.removeEventListener("select", vs._onScriptsChange, false);

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

  urls.sort(function(a, b) {
    return Math.random() - 0.5;
  });

  switch (method) {
    case 1:
      urls.forEach(function(url) {
        let loc = url.href + url.leaf;
        vs.addScript(ss._getScriptLabel(loc, url.href), { url: loc });
      });
      vs.commitScripts();
      break;

    case 2:
      urls.forEach(function(url) {
        let loc = url.href + url.leaf;
        vs.addScript(ss._getScriptLabel(loc, url.href), { url: loc }, true);
      });
      break;

    case 3:
      let i = 0
      for (; i < urls.length / 2; i++) {
        let url = urls[i];
        let loc = url.href + url.leaf;
        vs.addScript(ss._getScriptLabel(loc, url.href), { url: loc });
      }
      vs.commitScripts();

      for (; i < urls.length; i++) {
        let url = urls[i];
        let loc = url.href + url.leaf;
        vs.addScript(ss._getScriptLabel(loc, url.href), { url: loc }, true);
      }
      break;
  }

  executeSoon(function() {
    checkScriptsOrder(method);
    callback();
  });
}

function checkScriptsOrder(method) {
  let labels = gDebugger.DebuggerView.Scripts.scriptLabels;
  let sorted = labels.reduce(function(prev, curr, index, array) {
    return array[index - 1] < array[index];
  });

  ok(sorted,
    "Using method " + method + ", " +
    "the scripts weren't in the correct order: " + labels.toSource());

  return sorted;
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
