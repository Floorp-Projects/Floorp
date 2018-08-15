/* global browser */
"use strict";

function frameScript() {
  let frame = this.content.frames[0].frames[1];
  let docShell = frame.docShell;
  let controller = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsISelectionDisplay)
                           .QueryInterface(Ci.nsISelectionController);
  let selection = controller.getSelection(controller.SELECTION_FIND);
  let range = selection.getRangeAt(0);
  let scope = {};
  ChromeUtils.import("resource://gre/modules/FindContent.jsm", scope);
  let highlighter = (new scope.FindContent(docShell)).highlighter;
  let r1 = frame.parent.frameElement.getBoundingClientRect();
  let f1 = highlighter._getFrameElementOffsets(frame.parent);
  let r2 = frame.frameElement.getBoundingClientRect();
  let f2 = highlighter._getFrameElementOffsets(frame);
  let r3 = range.getBoundingClientRect();
  let rect = {
    top: (r1.top + r2.top + r3.top + f1.y + f2.y),
    left: (r1.left + r2.left + r3.left + f1.x + f2.x),
  };
  this.sendAsyncMessage("test:find:selectionTest", {text: selection.toString(), rect});
}

function waitForMessage(messageManager, topic) {
  return new Promise(resolve => {
    messageManager.addMessageListener(topic, function messageListener(message) {
      messageManager.removeMessageListener(topic, messageListener);
      resolve(message);
    });
  });
}

add_task(async function testDuplicatePinnedTab() {
  async function background() {
    function awaitLoad(tabId, url) {
      return new Promise(resolve => {
        browser.tabs.onUpdated.addListener(function listener(tabId_, changed, tab) {
          if (tabId == tabId_ && changed.status == "complete" && tab.url == url) {
            browser.tabs.onUpdated.removeListener(listener);
            resolve();
          }
        });
      });
    }

    let url = "http://example.com/browser/browser/components/extensions/test/browser/file_find_frames.html";
    let tab = await browser.tabs.update({url});
    await awaitLoad(tab.id, url);

    let data = await browser.find.find("banana", {includeRangeData: true});
    let rangeData = data.rangeData;

    browser.test.log("Test that `data.count` is the expected value.");
    browser.test.assertEq(6, data.count, "The value returned from `data.count`");

    browser.test.log("Test that `rangeData` has the proper number of values.");
    browser.test.assertEq(6, rangeData.length, "The number of values held in `rangeData`");

    browser.test.log("Test that the text found in the top window and nested frames corresponds to the proper position.");
    let terms = ["Banana", "bAnana", "baNana", "banAna", "banaNa", "bananA"];
    for (let i = 0; i < terms.length; i++) {
      browser.test.assertEq(terms[i], rangeData[i].text, `The text at range position ${i}:`);
    }

    browser.test.log("Test that case sensitive match works properly.");
    data = await browser.find.find("baNana", {caseSensitive: true, includeRangeData: true});
    browser.test.assertEq(1, data.count, "The number of matches found:");
    browser.test.assertEq("baNana", data.rangeData[0].text, "The text found:");

    browser.test.log("Test that case insensitive match works properly.");
    data = await browser.find.find("banana", {caseSensitive: false});
    browser.test.assertEq(6, data.count, "The number of matches found:");

    browser.test.log("Test that entire word match works properly.");
    data = await browser.find.find("banana", {entireWord: true});
    browser.test.assertEq(4, data.count, "The number of matches found, should skip 2 matches, \"banaNaland\" and \"bananAland\":");

    let expectedRangeData = [{framePos: 0, text: "example", startTextNodePos: 15, startOffset: 11, endTextNodePos: 15, endOffset: 18},
                             {framePos: 0, text: "example", startTextNodePos: 15, startOffset: 25, endTextNodePos: 15, endOffset: 32},
                             {framePos: 0, text: "example", startTextNodePos: 18, startOffset: 6, endTextNodePos: 18, endOffset: 13},
                             {framePos: 0, text: "example", startTextNodePos: 20, startOffset: 3, endTextNodePos: 20, endOffset: 10},
                             {framePos: 1, text: "example", startTextNodePos: 0, startOffset: 0, endTextNodePos: 0, endOffset: 7},
                             {framePos: 2, text: "example", startTextNodePos: 0, startOffset: 0, endTextNodePos: 0, endOffset: 7}];

    browser.test.log("Test that word found in the same node, different nodes and different frames returns the correct rangeData results.");
    data = await browser.find.find("example", {includeRangeData: true});
    for (let i = 0; i < data.rangeData.length; i++) {
      for (let name in data.rangeData[i]) {
        browser.test.assertEq(expectedRangeData[i][name], data.rangeData[i][name], `rangeData[${i}].${name}:`);
      }
    }

    browser.test.log("Test that `rangeData` is not returned if `includeRangeData` is false.");
    data = await browser.find.find("banana", {caseSensitive: false, includeRangeData: false});
    browser.test.assertEq(false, !!data.rangeData, "The boolean cast value of `rangeData`:");

    browser.test.log("Test that `rectData` is not returned if `includeRectData` is false.");
    data = await browser.find.find("banana", {caseSensitive: false, includeRectData: false});
    browser.test.assertEq(false, !!data.rectData, "The boolean cast value of `rectData`:");

    browser.test.log("Test that text spanning multiple inline elements is found.");
    data = await browser.find.find("fruitcake");
    browser.test.assertEq(1, data.count, "The number of matches found:");

    browser.test.log("Test that text spanning multiple block elements is not found.");
    data = await browser.find.find("angelfood");
    browser.test.assertEq(0, data.count, "The number of matches found:");

    browser.test.log("Test that `highlightResults` returns proper status code.");
    await browser.find.find("banana");

    await browser.test.assertRejects(browser.find.highlightResults({rangeIndex: 6}),
                                     /index supplied was out of range/,
                                     "rejected Promise should pass the expected error");

    data = await browser.find.find("xyz");
    await browser.test.assertRejects(browser.find.highlightResults({rangeIndex: 0}),
                                     /no search results to highlight/,
                                     "rejected Promise should pass the expected error");

    data = await browser.find.find("banana", {includeRectData: true});
    await browser.find.highlightResults({rangeIndex: 5});

    browser.test.sendMessage("test:find:WebExtensionFinished", data.rectData);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["find", "tabs"],
    },
    background,
  });

  await extension.startup();
  let rectData = await extension.awaitMessage("test:find:WebExtensionFinished");
  let {top, left} = rectData[5].rectsAndTexts.rectList[0];
  await extension.unload();

  let {selectedBrowser} = gBrowser;

  let frameScriptUrl = `data:,(${frameScript}).call(this)`;
  selectedBrowser.messageManager.loadFrameScript(frameScriptUrl, false, true);
  let message = await waitForMessage(selectedBrowser.messageManager, "test:find:selectionTest");

  info("Test that text was highlighted properly.");
  is(message.data.text, "bananA", `The text that was highlighted: - Expected: bananA, Actual: ${message.data.text}`);

  info("Test that rectangle data returned from the search matches the highlighted result.");
  is(message.data.rect.top, top, `rect.top: - Expected: ${message.data.rect.top}, Actual: ${top}`);
  is(message.data.rect.left, left, `rect.left: - Expected: ${message.data.rect.left}, Actual: ${left}`);
});

add_task(async function testAboutFind() {
  async function background() {
    await browser.test.assertRejects(
      browser.find.find("banana"),
      /Unable to search:/,
      "Should not be able to search about tabs");

    browser.test.sendMessage("done");
  }

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["find", "tabs"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});
