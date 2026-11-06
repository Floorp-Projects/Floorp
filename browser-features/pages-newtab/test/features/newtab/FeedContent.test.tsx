// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { createInstance } from "i18next";
import { I18nextProvider } from "react-i18next";
import { createRoot } from "react-dom/client";
import { flushSync } from "react-dom";
import { StandardUIProvider } from "../../../../../libs/ui/standard-provider.tsx";
import type { SafeNode } from "../../../../../libs/newtab/types.ts";
import { FeedContent } from "../../../src/features/newtab/Reader.tsx";
import { safeWebUrl } from "../../../src/features/newtab/ui.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

async function withRendered(
  nodes: SafeNode[],
  inspect: (container: HTMLElement) => void,
): Promise<void> {
  const i18n = createInstance();
  await i18n.init({
    lng: "en-US",
    fallbackLng: "en-US",
    resources: {},
    initImmediate: false,
  });
  const container = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "div",
  );
  document.documentElement.appendChild(container);
  const root = createRoot(container);
  try {
    flushSync(() =>
      root.render(
        <I18nextProvider i18n={i18n}>
          <StandardUIProvider>
            <FeedContent articleId="test-article" nodes={nodes} />
          </StandardUIProvider>
        </I18nextProvider>,
      ),
    );
    inspect(container);
  } finally {
    flushSync(() => root.unmount());
    container.remove();
  }
}
const tests: TestCase[] = [
  {
    name: "Feed content cannot turn text, unknown tags, or URL schemes into executable DOM",
    fn: async () => {
      const nodes = [
        {
          tag: "p",
          children: [
            {
              tag: "text",
              text: '<img src=x onerror="window.__feedExecuted=true">',
            },
          ],
        },
        {
          tag: "script",
          children: [{ tag: "text", text: "window.__feedExecuted=true" }],
        },
        {
          tag: "a",
          href: "javascript:window.__feedExecuted=true",
          children: [{ tag: "text", text: "bad link" }],
        },
        {
          tag: "a",
          href: "https://example.com/article",
          onclick: "window.__feedExecuted=true",
          children: [{ tag: "text", text: "good link" }],
        },
      ] as unknown as SafeNode[];
      await withRendered(nodes, (container) => {
        assertEquals(
          container.querySelectorAll(
            "script,iframe,object,img,[onclick],[onerror]",
          ).length,
          0,
          "Feed input must not create forbidden or automatically loaded elements",
        );
        assertEquals(
          container.querySelectorAll("a").length,
          1,
          "Only the HTTP link should be enabled",
        );
        assertEquals(
          container.querySelector("a")?.getAttribute("href"),
          "https://example.com/article",
          "The original safe link should remain",
        );
        assertEquals(
          container.querySelector("a")?.getAttribute("rel"),
          "noopener noreferrer",
          "External links must isolate the opener",
        );
        assert(
          container.textContent?.includes("<img src=x"),
          "Feed text must remain text",
        );
      });
    },
  },
  {
    name: "External feed images render placeholders without an automatic request",
    fn: async () => {
      await withRendered(
        [{ tag: "img", imageId: "remote-image", alt: "External illustration" }],
        (container) => {
          assertEquals(
            container.querySelectorAll("img").length,
            0,
            "External images must not load automatically",
          );
          assertEquals(
            container.querySelectorAll("button").length,
            1,
            "External images require an explicit action",
          );
          assert(
            container.textContent?.includes("Load this image"),
            "The explicit image-loading action should be available",
          );
        },
      );
    },
  },
  {
    name: "Article headings do not add another page h1",
    fn: async () => {
      await withRendered(
        [{ tag: "h1", children: [{ tag: "text", text: "Feed heading" }] }],
        (container) => {
          assertEquals(
            container.querySelectorAll("h1").length,
            0,
            "The article title remains the only page h1",
          );
          assertEquals(
            container.querySelector("h2")?.textContent,
            "Feed heading",
            "Feed headings should be below the article title",
          );
        },
      );
    },
  },
  {
    name: "Only anonymous HTTP(S) links are enabled in the page",
    fn: () => {
      for (const address of [
        "javascript:alert(1)",
        "file:///tmp/a",
        "chrome://browser/content/browser.xhtml",
        "data:text/html,test",
        "https://user:secret@example.com/",
      ])
        assertEquals(
          safeWebUrl(address),
          null,
          `Unsafe or credential-bearing link should be disabled: ${address}`,
        );
      assertEquals(
        safeWebUrl("https://example.com/feed?token=kept#article"),
        "https://example.com/feed?token=kept#article",
        "Safe query and fragment values must not be silently removed",
      );
    },
  },
];
export async function runAllTests(): Promise<void> {
  await runTests("FeedContent.test.tsx", tests);
}
