/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that urls are correctly sorted when added to the sources widget.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gSources, gUtils;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gUtils = gDebugger.SourceUtils;

    waitForSourceShown(gPanel, ".html").then(() => {
      addSourceAndCheckOrder(1, () => {
        addSourceAndCheckOrder(2, () => {
          addSourceAndCheckOrder(3, () => {
            closeDebuggerAndFinish(gPanel);
          });
        });
      });
    });
  });
}

function addSourceAndCheckOrder(aMethod, aCallback) {
  gSources.empty();

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

  switch (aMethod) {
    case 1:
      for (let { href, leaf } of urls) {
        let url = href + leaf;
        let label = gUtils.getSourceLabel(url);
        gSources.push([label, url], { staged: true });
      }
      gSources.commit({ sorted: true });
      break;

    case 2:
      for (let { href, leaf } of urls) {
        let url = href + leaf;
        let label = gUtils.getSourceLabel(url);
        gSources.push([label, url], { staged: false });
      }
      break;

    case 3:
      let i = 0
      for (; i < urls.length / 2; i++) {
        let { href, leaf } = urls[i];
        let url = href + leaf;
        let label = gUtils.getSourceLabel(url);
        gSources.push([label, url], { staged: true });
      }
      gSources.commit({ sorted: true });

      for (; i < urls.length; i++) {
        let { href, leaf } = urls[i];
        let url = href + leaf;
        let label = gUtils.getSourceLabel(url);
        gSources.push([label, url], { staged: false });
      }
      break;
  }

  checkSourcesOrder(aMethod);
  aCallback();
}

function checkSourcesOrder(aMethod) {
  let labels = gSources.labels;
  let sorted = labels.reduce(function(aPrev, aCurr, aIndex, aArray) {
    return aArray[aIndex - 1] < aArray[aIndex];
  });

  ok(sorted,
    "Using method " + aMethod + ", " +
    "the sources weren't in the correct order: " + labels.toSource());

  return sorted;
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
  gUtils = null;
});
