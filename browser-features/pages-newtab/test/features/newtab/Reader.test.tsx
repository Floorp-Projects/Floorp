// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { createInstance } from "i18next";
import { I18nextProvider } from "react-i18next";
import { createRoot } from "react-dom/client";
import { flushSync } from "react-dom";
import { StandardUIProvider } from "../../../../../libs/ui/standard-provider.tsx";
import { newTabClient } from "../../../../../libs/newtab/client.ts";
import { createDefaultNewTabSettings } from "../../../../../libs/newtab/settings.ts";
import type {
  Article,
  NewTabMethod,
  NewTabOperations,
  NewTabSnapshot,
} from "../../../../../libs/newtab/types.ts";
import { Reader } from "../../../src/features/newtab/Reader.tsx";
import { NewTabProvider } from "../../../src/features/newtab/state.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function article(id: string): Article {
  return {
    id,
    feedId: "reader-test-feed",
    title: `Article ${id}`,
    link: `https://example.com/${id}`,
    author: "",
    publishedAt: null,
    receivedAt: 1_700_000_000_000,
    excerpt: `Excerpt ${id}`,
    isRead: true,
    starred: false,
    hasFullContent: true,
    content: [{ tag: "p", children: [{ tag: "text", text: `Body ${id}` }] }],
  };
}

async function until(condition: () => boolean, message: string): Promise<void> {
  const deadline = performance.now() + 3000;
  while (!condition() && performance.now() < deadline)
    await new Promise((resolve) => setTimeout(resolve, 20));
  assert(condition(), message);
}

function element<T extends HTMLElement>(
  container: HTMLElement,
  selector: string,
): T {
  const result = container.querySelector<T>(selector);
  assert(result, `Expected ${selector}`);
  return result;
}

async function withReader(
  getArticle: (id: string) => Promise<Article>,
  inspect: (container: HTMLElement) => Promise<void>,
): Promise<void> {
  const snapshot: NewTabSnapshot = {
    settings: createDefaultNewTabSettings(),
    groups: [],
    workspace: null,
    workspaces: [],
    sponsors: [],
    feeds: [
      {
        id: "reader-test-feed",
        url: "https://example.com/feed.xml",
        title: "Reader test feed",
        siteUrl: "https://example.com/",
        enabled: true,
        unreadCount: 0,
        lastSuccessAt: null,
        error: null,
      },
    ],
    readOnly: false,
    private: false,
  };
  const originalCall = newTabClient.call;
  const originalSubscribe = newTabClient.subscribe;
  const originalUrl = location.href;
  const originalHistoryState: unknown = history.state;
  const previousFocus = document.activeElement;
  const container = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "div",
  );
  container.style.cssText =
    "position:fixed;inset:0;overflow:auto;background:white;z-index:2147483647";
  document.documentElement.appendChild(container);
  const root = createRoot(container);
  const i18n = createInstance();
  await i18n.init({ lng: "en-US", resources: {}, initImmediate: false });
  try {
    history.replaceState(null, "", "#feed");
    newTabClient.subscribe = () => () => {};
    newTabClient.call = async <K extends NewTabMethod>(
      method: K,
      params: NewTabOperations[K]["input"],
    ): Promise<NewTabOperations[K]["output"]> => {
      let result: unknown;
      if (method === "snapshot") result = snapshot;
      else if (method === "articles")
        result = {
          items: [article("first"), article("second"), article("third")],
          total: 3,
          nextOffset: null,
        };
      else if (method === "article" && "id" in params)
        result = await getArticle(params.id);
      else throw new Error(`Unexpected reader request: ${method}`);
      return result as NewTabOperations[K]["output"];
    };
    flushSync(() =>
      root.render(
        <I18nextProvider i18n={i18n}>
          <StandardUIProvider>
            <NewTabProvider>
              <Reader />
            </NewTabProvider>
          </StandardUIProvider>
        </I18nextProvider>,
      ),
    );
    await until(
      () => container.querySelectorAll("[data-article-id]").length === 3,
      "Reader renders the article list",
    );
    await inspect(container);
  } finally {
    flushSync(() => root.unmount());
    container.remove();
    newTabClient.call = originalCall;
    newTabClient.subscribe = originalSubscribe;
    history.replaceState(originalHistoryState, "", originalUrl);
    if (previousFocus instanceof HTMLElement && previousFocus.isConnected)
      previousFocus.focus({ preventScroll: true });
  }
}

const tests: TestCase[] = [
  {
    name: "Returning from an article restores list focus, selection and scroll position",
    fn: () =>
      withReader(
        (id) => Promise.resolve(article(id)),
        async (container) => {
          const list = element<HTMLDivElement>(
            container,
            ".nt-reader-list-scroll",
          );
          list.style.cssText = "height:80px;overflow-y:auto";
          const selected = element<HTMLButtonElement>(
            container,
            '[data-article-id="second"]',
          );
          selected.focus();
          list.scrollTop = 30;
          const scrollTop = list.scrollTop;
          assert(scrollTop > 0, "The fixture has a scrollable list");
          flushSync(() => selected.click());
          const body = element<HTMLDivElement>(
            container,
            ".nt-reader-article-scroll",
          );
          await until(
            () =>
              document.activeElement === body &&
              Boolean(body.querySelector("h1")),
            "Opening the article moves focus into its pane",
          );
          flushSync(() =>
            element<HTMLButtonElement>(container, ".nt-back-to-list").click(),
          );
          await until(
            () => document.activeElement === selected,
            "Back restores the originating list button",
          );
          assertEquals(
            selected.getAttribute("aria-current"),
            "true",
            "The previously read row stays selected",
          );
          assertEquals(
            list.scrollTop,
            scrollTop,
            "Returning does not reset the article list's scroll position",
          );
          assertEquals(
            body.getAttribute("aria-busy"),
            "false",
            "Closed articles do not leave a stale loading state",
          );
        },
      ),
  },
  {
    name: "Changing articles removes the old article's actions while the next body is loading",
    fn: async () => {
      let resolveSecond: ((value: Article) => void) | undefined;
      const second = new Promise<Article>((resolve) => {
        resolveSecond = resolve;
      });
      await withReader(
        (id) => (id === "second" ? second : Promise.resolve(article(id))),
        async (container) => {
          flushSync(() =>
            element<HTMLButtonElement>(
              container,
              '[data-article-id="first"]',
            ).click(),
          );
          await until(
            () =>
              Boolean(
                container.querySelector(
                  ".nt-reader-article-toolbar .nt-external-button",
                ),
              ),
            "The first article exposes its actions",
          );
          flushSync(() =>
            element<HTMLButtonElement>(
              container,
              '[data-article-id="second"]',
            ).click(),
          );
          await until(
            () =>
              !container.querySelector(
                ".nt-reader-article-toolbar .nt-external-button",
              ),
            "The previous article's original-site action is removed during loading",
          );
          assertEquals(
            container.querySelector(
              ".nt-reader-article-toolbar [aria-pressed]",
            ),
            null,
            "The previous article's star action cannot be invoked during loading",
          );
          assertEquals(
            element(container, ".nt-reader-article-scroll").getAttribute(
              "aria-busy",
            ),
            "true",
            "The new article reports its pending state",
          );
          resolveSecond?.(article("second"));
          await until(
            () =>
              container.querySelector(".nt-article-content h1")?.textContent ===
              "Article second",
            "The requested article replaces the pending state",
          );
        },
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("Reader.test.tsx", tests);
}
