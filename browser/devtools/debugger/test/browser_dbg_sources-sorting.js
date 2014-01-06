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
  gSources.suppressSelectionEvents = true;

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
        let dummy = document.createElement("label");
        gSources.push([dummy, url], {
          staged: true,
          attachment: {
            label: label
          }
        });
      }
      gSources.commit({ sorted: true });
      break;

    case 2:
      for (let { href, leaf } of urls) {
        let url = href + leaf;
        let label = gUtils.getSourceLabel(url);
        let dummy = document.createElement("label");
        gSources.push([dummy, url], {
          staged: false,
          attachment: {
            label: label
          }
        });
      }
      break;

    case 3:
      let i = 0
      for (; i < urls.length / 2; i++) {
        let { href, leaf } = urls[i];
        let url = href + leaf;
        let label = gUtils.getSourceLabel(url);
        let dummy = document.createElement("label");
        gSources.push([dummy, url], {
          staged: true,
          attachment: {
            label: label
          }
        });
      }
      gSources.commit({ sorted: true });

      for (; i < urls.length; i++) {
        let { href, leaf } = urls[i];
        let url = href + leaf;
        let label = gUtils.getSourceLabel(url);
        let dummy = document.createElement("label");
        gSources.push([dummy, url], {
          staged: false,
          attachment: {
            label: label
          }
        });
      }
      break;
  }

  checkSourcesOrder(aMethod);
  aCallback();
}

function checkSourcesOrder(aMethod) {
  let attachments = gSources.attachments;

  for (let i = 0; i < attachments.length - 1; i++) {
    let first = attachments[i].label;
    let second = attachments[i + 1].label;
    ok(first < second,
      "Using method " + aMethod + ", " +
      "the sources weren't in the correct order: " + first + " vs. " + second);
  }
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
  gUtils = null;
});
