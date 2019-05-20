"use strict";

const {ASRouter} = ChromeUtils.import("resource://activity-stream/lib/ASRouter.jsm");

test_newtab({
  async before() {
    let data = ASRouter.state.messages.find(m => m.id === "SIMPLE_BELOW_SEARCH_TEST_1");
    ASRouter.messageChannel.sendAsyncMessage("ASRouter:parent-to-child", {type: "SET_MESSAGE", data});
  },
  test: async function test_simple_below_search_snippet() {
    // Verify the simple_below_search_snippet renders in container below searchbox
    // and nothing is rendered in the footer.
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".below-search-snippet .SimpleBelowSearchSnippet"),
      "Should find the snippet inside the below search container");

    is(0, content.document.querySelector("#footer-asrouter-container").childNodes.length,
      "Should not find any snippets in the footer container");
  },
});

test_newtab({
  async before() {
    let data = ASRouter.state.messages.find(m => m.id === "SIMPLE_TEST_1");
    ASRouter.messageChannel.sendAsyncMessage("ASRouter:parent-to-child", {type: "SET_MESSAGE", data});
  },
  test: async function test_simple_snippet() {
    // Verify the simple_snippet renders in the footer and the container below
    // searchbox is not rendered.
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector("#footer-asrouter-container .SimpleSnippet"),
      "Should find the snippet inside the footer container");

    ok(!content.document.querySelector(".below-search-snippet"),
      "Should not find any snippets below search");
  },
});
