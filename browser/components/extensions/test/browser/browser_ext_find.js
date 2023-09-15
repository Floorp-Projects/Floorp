/* global browser */
"use strict";

function frameScript() {
  let docShell = content.docShell;
  let controller = docShell
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsISelectionDisplay)
    .QueryInterface(Ci.nsISelectionController);
  let selection = controller.getSelection(controller.SELECTION_FIND);
  if (!selection.rangeCount) {
    return {
      text: "",
    };
  }

  let range = selection.getRangeAt(0);
  const { FindContent } = ChromeUtils.importESModule(
    "resource://gre/modules/FindContent.sys.mjs"
  );
  let highlighter = new FindContent(docShell).highlighter;
  let r1 = content.parent.frameElement.getBoundingClientRect();
  let f1 = highlighter._getFrameElementOffsets(content.parent);
  let r2 = content.frameElement.getBoundingClientRect();
  let f2 = highlighter._getFrameElementOffsets(content);
  let r3 = range.getBoundingClientRect();
  let rect = {
    top: r1.top + r2.top + r3.top + f1.y + f2.y,
    left: r1.left + r2.left + r3.left + f1.x + f2.x,
  };
  return {
    text: selection.toString(),
    rect,
  };
}

add_task(async function testFind() {
  async function background() {
    function awaitLoad(tabId, url) {
      return new Promise(resolve => {
        browser.tabs.onUpdated.addListener(function listener(
          tabId_,
          changed,
          tab
        ) {
          if (
            tabId == tabId_ &&
            changed.status == "complete" &&
            tab.url == url
          ) {
            browser.tabs.onUpdated.removeListener(listener);
            resolve();
          }
        });
      });
    }

    let url =
      "http://example.com/browser/browser/components/extensions/test/browser/file_find_frames.html";
    let tab = await browser.tabs.update({ url });
    await awaitLoad(tab.id, url);

    let data = await browser.find.find("banana", { includeRangeData: true });
    let rangeData = data.rangeData;

    browser.test.log("Test that `data.count` is the expected value.");
    browser.test.assertEq(
      6,
      data.count,
      "The value returned from `data.count`"
    );

    browser.test.log("Test that `rangeData` has the proper number of values.");
    browser.test.assertEq(
      6,
      rangeData.length,
      "The number of values held in `rangeData`"
    );

    browser.test.log(
      "Test that the text found in the top window and nested frames corresponds to the proper position."
    );
    let terms = ["Bánana", "bAnana", "baNana", "banAna", "banaNa", "bananA"];
    for (let i = 0; i < terms.length; i++) {
      browser.test.assertEq(
        terms[i],
        rangeData[i].text,
        `The text at range position ${i}:`
      );
    }

    browser.test.log("Test that case sensitive match works properly.");
    data = await browser.find.find("baNana", {
      caseSensitive: true,
      includeRangeData: true,
    });
    browser.test.assertEq(1, data.count, "The number of matches found:");
    browser.test.assertEq("baNana", data.rangeData[0].text, "The text found:");

    browser.test.log("Test that diacritic sensitive match works properly.");
    data = await browser.find.find("bánana", {
      matchDiacritics: true,
      includeRangeData: true,
    });
    browser.test.assertEq(1, data.count, "The number of matches found:");
    browser.test.assertEq("Bánana", data.rangeData[0].text, "The text found:");

    browser.test.log("Test that case insensitive match works properly.");
    data = await browser.find.find("banana", { caseSensitive: false });
    browser.test.assertEq(6, data.count, "The number of matches found:");

    browser.test.log("Test that entire word match works properly.");
    data = await browser.find.find("banana", { entireWord: true });
    browser.test.assertEq(
      4,
      data.count,
      'The number of matches found, should skip 2 matches, "banaNaland" and "bananAland":'
    );

    let expectedRangeData = [
      {
        framePos: 0,
        text: "example",
        startTextNodePos: 16,
        startOffset: 11,
        endTextNodePos: 16,
        endOffset: 18,
      },
      {
        framePos: 0,
        text: "example",
        startTextNodePos: 16,
        startOffset: 25,
        endTextNodePos: 16,
        endOffset: 32,
      },
      {
        framePos: 0,
        text: "example",
        startTextNodePos: 19,
        startOffset: 6,
        endTextNodePos: 19,
        endOffset: 13,
      },
      {
        framePos: 0,
        text: "example",
        startTextNodePos: 21,
        startOffset: 3,
        endTextNodePos: 21,
        endOffset: 10,
      },
      {
        framePos: 1,
        text: "example",
        startTextNodePos: 0,
        startOffset: 0,
        endTextNodePos: 0,
        endOffset: 7,
      },
      {
        framePos: 2,
        text: "example",
        startTextNodePos: 0,
        startOffset: 0,
        endTextNodePos: 0,
        endOffset: 7,
      },
    ];

    browser.test.log(
      "Test that word found in the same node, different nodes and different frames returns the correct rangeData results."
    );
    data = await browser.find.find("example", { includeRangeData: true });
    for (let i = 0; i < data.rangeData.length; i++) {
      for (let name in data.rangeData[i]) {
        browser.test.assertEq(
          expectedRangeData[i][name],
          data.rangeData[i][name],
          `rangeData[${i}].${name}:`
        );
      }
    }

    browser.test.log(
      "Test that `rangeData` is not returned if `includeRangeData` is false."
    );
    data = await browser.find.find("banana", {
      caseSensitive: false,
      includeRangeData: false,
    });
    browser.test.assertEq(
      false,
      !!data.rangeData,
      "The boolean cast value of `rangeData`:"
    );

    browser.test.log(
      "Test that `rectData` is not returned if `includeRectData` is false."
    );
    data = await browser.find.find("banana", {
      caseSensitive: false,
      includeRectData: false,
    });
    browser.test.assertEq(
      false,
      !!data.rectData,
      "The boolean cast value of `rectData`:"
    );

    browser.test.log(
      "Test that text spanning multiple inline elements is found."
    );
    data = await browser.find.find("fruitcake");
    browser.test.assertEq(1, data.count, "The number of matches found:");

    browser.test.log(
      "Test that text spanning multiple block elements is not found."
    );
    data = await browser.find.find("angelfood");
    browser.test.assertEq(0, data.count, "The number of matches found:");

    browser.test.log(
      "Test that `highlightResults` returns proper status code."
    );
    await browser.find.find("banana");

    await browser.test.assertRejects(
      browser.find.highlightResults({ rangeIndex: 6 }),
      /index supplied was out of range/,
      "rejected Promise should pass the expected error"
    );

    data = await browser.find.find("xyz");
    await browser.test.assertRejects(
      browser.find.highlightResults({ rangeIndex: 0 }),
      /no search results to highlight/,
      "rejected Promise should pass the expected error"
    );

    // Test highlightResults without any arguments, especially `rangeIndex`.
    data = await browser.find.find("example");
    browser.test.assertEq(6, data.count, "The number of matches found:");
    await browser.find.highlightResults();

    await browser.find.removeHighlighting();

    data = await browser.find.find("banana", { includeRectData: true });
    await browser.find.highlightResults({ rangeIndex: 5 });

    browser.test.sendMessage("test:find:WebExtensionFinished", data.rectData);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["find", "tabs"],
    },
    background,
  });

  await extension.startup();
  let rectData = await extension.awaitMessage("test:find:WebExtensionFinished");
  let { top, left } = rectData[5].rectsAndTexts.rectList[0];
  await extension.unload();

  let subFrameBrowsingContext =
    gBrowser.selectedBrowser.browsingContext.children[0].children[1];
  let result = await SpecialPowers.spawn(
    subFrameBrowsingContext,
    [],
    frameScript
  );

  info("Test that text was highlighted properly.");
  is(
    result.text,
    "bananA",
    `The text that was highlighted: - Expected: bananA, Actual: ${result.text}`
  );

  info(
    "Test that rectangle data returned from the search matches the highlighted result."
  );
  is(
    result.rect.top,
    top,
    `rect.top: - Expected: ${result.rect.top}, Actual: ${top}`
  );
  is(
    result.rect.left,
    left,
    `rect.left: - Expected: ${result.rect.left}, Actual: ${left}`
  );
});

add_task(async function testRemoveHighlighting() {
  async function background() {
    function awaitLoad(tabId, url) {
      return new Promise(resolve => {
        browser.tabs.onUpdated.addListener(function listener(
          tabId_,
          changed,
          tab
        ) {
          if (
            tabId == tabId_ &&
            changed.status == "complete" &&
            tab.url == url
          ) {
            browser.tabs.onUpdated.removeListener(listener);
            resolve();
          }
        });
      });
    }

    let url =
      "http://example.com/browser/browser/components/extensions/test/browser/file_find_frames.html";
    let tab = await browser.tabs.update({ url });
    await awaitLoad(tab.id, url);

    let data = await browser.find.find("banana", { includeRangeData: true });

    browser.test.log("Test that `data.count` is the expected value.");
    browser.test.assertEq(
      6,
      data.count,
      "The value returned from `data.count`"
    );

    await browser.find.highlightResults({ rangeIndex: 5 });

    browser.find.removeHighlighting();

    browser.test.sendMessage("test:find:WebExtensionFinished");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["find", "tabs"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("test:find:WebExtensionFinished");
  await extension.unload();

  let subFrameBrowsingContext =
    gBrowser.selectedBrowser.browsingContext.children[0].children[1];
  let result = await SpecialPowers.spawn(
    subFrameBrowsingContext,
    [],
    frameScript
  );

  info("Test that highlight was cleared properly.");
  is(
    result.text,
    "",
    `The text that was highlighted: - Expected: '', Actual: ${result.text}`
  );
});

add_task(async function testAboutFind() {
  async function background() {
    await browser.test.assertRejects(
      browser.find.find("banana"),
      /Unable to search:/,
      "Should not be able to search about tabs"
    );

    browser.test.sendMessage("done");
  }

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["find", "tabs"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testIncognitoFind() {
  async function background() {
    await browser.test.assertRejects(
      browser.find.find("banana"),
      /Unable to search:/,
      "Should not be able to search private window"
    );
    await browser.test.assertRejects(
      browser.find.highlightResults(),
      /Unable to search:/,
      "Should not be able to highlight in private window"
    );
    await browser.test.assertRejects(
      browser.find.removeHighlighting(),
      /Invalid tab ID:/,
      "Should not be able to remove highlight in private window"
    );

    browser.test.sendMessage("done");
  }

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  BrowserTestUtils.startLoadingURIString(
    privateWin.gBrowser.selectedBrowser,
    "http://example.com"
  );
  await BrowserTestUtils.browserLoaded(privateWin.gBrowser.selectedBrowser);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["find", "tabs"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function testIncognitoFindAllowed() {
  // We're only testing we can make the calls in a private window,
  // testFind above tests full functionality.
  async function background() {
    await browser.find.find("banana");
    await browser.find.highlightResults({ rangeIndex: 0 });
    await browser.find.removeHighlighting();

    browser.test.sendMessage("done");
  }

  let url =
    "http://example.com/browser/browser/components/extensions/test/browser/file_find_frames.html";
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  BrowserTestUtils.startLoadingURIString(
    privateWin.gBrowser.selectedBrowser,
    url
  );
  await BrowserTestUtils.browserLoaded(privateWin.gBrowser.selectedBrowser);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["find", "tabs"],
    },
    background,
    incognitoOverride: "spanning",
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
  await BrowserTestUtils.closeWindow(privateWin);
});
