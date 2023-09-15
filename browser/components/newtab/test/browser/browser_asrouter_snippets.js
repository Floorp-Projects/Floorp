"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const { TelemetryFeed } = ChromeUtils.import(
  "resource://activity-stream/lib/TelemetryFeed.jsm"
);

add_task(async function render_below_search_snippet() {
  ASRouter._validPreviewEndpoint = () => true;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:newtab?endpoint=https://example.com/browser/browser/components/newtab/test/browser/snippet_below_search_test.json",
    },
    async browser => {
      await waitForPreloaded(browser);

      const complete = await SpecialPowers.spawn(browser, [], async () => {
        // Verify the simple_below_search_snippet renders in container below searchbox
        // and nothing is rendered in the footer.
        await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector(
              ".below-search-snippet .SimpleBelowSearchSnippet"
            ),
          "Should find the snippet inside the below search container"
        );

        is(
          0,
          content.document.querySelector("#footer-asrouter-container")
            .childNodes.length,
          "Should not find any snippets in the footer container"
        );

        return true;
      });

      Assert.ok(complete, "Test complete.");
    }
  );
});

add_task(async function render_snippets_icon_and_link() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:newtab?endpoint=https://example.com/browser/browser/components/newtab/test/browser/snippet_simple_test.json",
    },
    async browser => {
      await waitForPreloaded(browser);

      const complete = await SpecialPowers.spawn(browser, [], async () => {
        const syncLink = "https://www.mozilla.org/en-US/firefox/accounts";
        // Verify the simple_snippet renders in the footer and the container below
        // searchbox is not rendered.
        await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector(
              "#footer-asrouter-container .SimpleSnippet"
            ),
          "Should find the snippet inside the footer container"
        );
        await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector(
              "#footer-asrouter-container .SimpleSnippet .icon"
            ),
          "Should render an icon"
        );
        await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector(
              `#footer-asrouter-container .SimpleSnippet a[href='${syncLink}']`
            ),
          "Should render an anchor with the correct href"
        );

        ok(
          !content.document.querySelector(".below-search-snippet"),
          "Should not find any snippets below search"
        );

        return true;
      });

      Assert.ok(complete, "Test complete.");
    }
  );
});

add_task(async function render_preview_snippet() {
  ASRouter._validPreviewEndpoint = () => true;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:newtab?endpoint=https://example.com/browser/browser/components/newtab/test/browser/snippet.json",
    },
    async browser => {
      let text = await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(".activity-stream"),
          `Should render Activity Stream`
        );
        await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector(
              "#footer-asrouter-container .SimpleSnippet"
            ),
          "Should find the snippet inside the footer container"
        );

        return content.document.querySelector(
          "#footer-asrouter-container .SimpleSnippet"
        ).innerText;
      });

      Assert.equal(
        text,
        "On January 30th Nightly will introduce dedicated profiles, making it simpler to run different installations of Firefox side by side. Learn what this means for you.",
        "Snippet content match"
      );
    }
  );
});

add_task(async function test_snippets_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.snippets",
        `{"id":"snippets","enabled":true,"type":"remote","url":"https://example.com/browser/browser/components/newtab/test/browser/snippet.json","updateCycleInMs":0}`,
      ],
      ["browser.newtabpage.activity-stream.feeds.snippets", true],
    ],
  });
  const sendPingStub = sinon.stub(
    TelemetryFeed.prototype,
    "sendStructuredIngestionEvent"
  );
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      // Work around any issues caching might introduce by navigating to
      // about blank first
      url: "about:blank",
    },
    async browser => {
      BrowserTestUtils.startLoadingURIString(browser, "about:home");
      await BrowserTestUtils.browserLoaded(browser);
      let text = await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(".activity-stream"),
          `Should render Activity Stream`
        );
        await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector(
              "#footer-asrouter-container .SimpleSnippet"
            ),
          "Should find the snippet inside the footer container"
        );

        return content.document.querySelector(
          "#footer-asrouter-container .SimpleSnippet"
        ).innerText;
      });

      Assert.equal(
        text,
        "On January 30th Nightly will introduce dedicated profiles, making it simpler to run different installations of Firefox side by side. Learn what this means for you.",
        "Snippet content match"
      );
    }
  );

  Assert.ok(sendPingStub.callCount >= 1, "We registered some pings");
  const snippetsPing = sendPingStub.args.find(args => args[2] === "snippets");
  Assert.ok(snippetsPing, "Found the snippets ping");
  Assert.equal(
    snippetsPing[0].event,
    "IMPRESSION",
    "It's the correct ping type"
  );

  sendPingStub.restore();
});
