// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  isSameSilverPassDocument,
  isSilverPassDemoUrl,
  SILVER_PASS_DEMO_MARKER,
  SILVER_PASS_DEMO_URL,
  SilverPassManager,
} from "../silver-pass-manager.ts";
import type { SilverPassPageClient, SilverPassPageContext } from "../types.ts";

const TARGET_SELECTOR = '#apply-start[data-silver-pass-target="apply-start"]';
const COMPLETE_DEMO_TEXT = [
  "高齢者移動支援券",
  "住民税非課税世帯",
  "交付申請",
  "本人確認書類",
  "利用者証明用電子証明書",
  "所得・課税証明書",
].join(" ");

interface ClientBehavior {
  waitForReady?: () => Promise<boolean>;
  isVisible?: (selector: string) => Promise<boolean>;
  getScopedText?: (selector: string) => Promise<string | null>;
  clearEffects?: () => Promise<boolean>;
  scrollToElement?: (selector: string) => Promise<boolean>;
}

class RecordingClient implements SilverPassPageClient {
  readonly calls: string[] = [];

  constructor(private readonly behavior: ClientBehavior = {}) {}

  waitForReady(): Promise<boolean> {
    this.calls.push("waitForReady");
    return this.behavior.waitForReady?.() ?? Promise.resolve(true);
  }

  isVisible(selector: string): Promise<boolean> {
    this.calls.push(`isVisible:${selector}`);
    return this.behavior.isVisible?.(selector) ?? Promise.resolve(true);
  }

  getScopedText(selector: string): Promise<string | null> {
    this.calls.push(`getScopedText:${selector}`);
    return this.behavior.getScopedText?.(selector) ??
      Promise.resolve(COMPLETE_DEMO_TEXT);
  }

  clearEffects(): Promise<boolean> {
    this.calls.push("clearEffects");
    return this.behavior.clearEffects?.() ?? Promise.resolve(true);
  }

  scrollToElement(selector: string): Promise<boolean> {
    this.calls.push(`scrollToElement:${selector}`);
    return this.behavior.scrollToElement?.(selector) ?? Promise.resolve(true);
  }
}

interface ContextFixture {
  page: SilverPassPageContext;
  createClientCount: () => number;
}

function createContextFixture(
  client: SilverPassPageClient | null,
  url = SILVER_PASS_DEMO_URL,
): ContextFixture {
  let count = 0;
  return {
    page: {
      url,
      createClient() {
        count += 1;
        return client;
      },
    },
    createClientCount: () => count,
  };
}

function assertCalls(
  actual: readonly string[],
  expected: readonly string[],
  message: string,
): void {
  assertEquals(
    JSON.stringify(actual),
    JSON.stringify(expected),
    message,
  );
}

function createDeferred<T>(): {
  promise: Promise<T>;
  resolve: (value: T) => void;
} {
  let resolvePromise: (value: T) => void = () => {
    throw new Error("deferred promise resolved before initialization");
  };
  const promise = new Promise<T>((resolve) => {
    resolvePromise = resolve;
  });
  return { promise, resolve: resolvePromise };
}

const tests: TestCase[] = [
  {
    name: "URL gate accepts only the exact local demo URL and fragments",
    fn() {
      assertEquals(
        isSilverPassDemoUrl(SILVER_PASS_DEMO_URL),
        true,
        "the exact fixture URL should be supported",
      );
      assertEquals(
        isSilverPassDemoUrl(`${SILVER_PASS_DEMO_URL}#online`),
        true,
        "a fragment should not change the supported document",
      );
      assertEquals(
        isSameSilverPassDocument(
          `${SILVER_PASS_DEMO_URL}#top`,
          `${SILVER_PASS_DEMO_URL}#overview`,
        ),
        true,
        "fragment changes should keep an in-flight operation on the same document",
      );
      assertEquals(
        isSameSilverPassDocument(
          `${SILVER_PASS_DEMO_URL}?source=test#top`,
          `${SILVER_PASS_DEMO_URL}#overview`,
        ),
        false,
        "query changes must still invalidate an in-flight operation",
      );
      assertEquals(
        isSameSilverPassDocument(
          "http://127.1:4173/municipal-application.html#top",
          `${SILVER_PASS_DEMO_URL}#overview`,
        ),
        false,
        "IPv4 shorthand must not pass the in-flight raw URL guard",
      );
      assertEquals(
        isSameSilverPassDocument(
          "http://2130706433:4173/municipal-application.html#top",
          `${SILVER_PASS_DEMO_URL}#overview`,
        ),
        false,
        "integer IPv4 notation must not pass the in-flight raw URL guard",
      );
      assertEquals(
        isSameSilverPassDocument(
          "http://127.0.0.1:4173/a/../municipal-application.html#top",
          `${SILVER_PASS_DEMO_URL}#overview`,
        ),
        false,
        "dot segments must not pass the in-flight raw URL guard",
      );

      const rejectedUrls = [
        `${SILVER_PASS_DEMO_URL}?source=test`,
        "http://127.0.0.1:4173/",
        "http://localhost:4173/municipal-application.html",
        "https://127.0.0.1:4173/municipal-application.html",
        "file:///tmp/municipal-application.html",
        "http://user:pass@127.0.0.1:4173/municipal-application.html",
        "http://127.0.0.1:4173/a/../municipal-application.html",
        "http://127.1:4173/municipal-application.html",
        "http://2130706433:4173/municipal-application.html",
      ];
      for (const url of rejectedUrls) {
        assertEquals(
          isSilverPassDemoUrl(url),
          false,
          `the URL gate should reject ${url}`,
        );
      }
    },
  },
  {
    name: "unsupported URL never acquires or calls the page client",
    async fn() {
      const client = new RecordingClient();
      const fixture = createContextFixture(client, "https://example.com/");
      const manager = new SilverPassManager(() => fixture.page);

      await manager.analyzeCurrentPage();

      assertEquals(
        fixture.createClientCount(),
        0,
        "an unsupported URL must be rejected before actor acquisition",
      );
      assertCalls(
        client.calls,
        [],
        "an unsupported URL must not query content",
      );
      assertEquals(
        manager.state().status,
        "unsupported",
        "the panel should report unsupported content",
      );
    },
  },
  {
    name: "a stale allowlisted page is rejected before actor acquisition",
    async fn() {
      const client = new RecordingClient();
      const fixture = createContextFixture(client);
      fixture.page.isCurrent = () => false;
      const manager = new SilverPassManager(() => fixture.page);

      await manager.analyzeCurrentPage();

      assertEquals(
        fixture.createClientCount(),
        0,
        "a stale page must not acquire the actor client",
      );
      assertCalls(
        client.calls,
        [],
        "a stale page must not send even a readiness query",
      );
      assertEquals(
        manager.state().status,
        "unsupported",
        "the panel should stop loading after the page becomes stale",
      );
    },
  },
  {
    name: "missing marker stops before effects, text, target, or scrolling",
    async fn() {
      const client = new RecordingClient({
        isVisible: (selector) =>
          Promise.resolve(selector !== SILVER_PASS_DEMO_MARKER),
      });
      const fixture = createContextFixture(client);
      const manager = new SilverPassManager(() => fixture.page);

      await manager.analyzeCurrentPage();

      assertCalls(
        client.calls,
        ["waitForReady", `isVisible:${SILVER_PASS_DEMO_MARKER}`],
        "a missing marker must stop all scoped content access",
      );
      assertEquals(
        manager.state().status,
        "unsupported",
        "a marker mismatch should be unsupported, not a generic error",
      );
    },
  },
  {
    name: "successful analysis follows the safe query order and becomes ready",
    async fn() {
      const client = new RecordingClient();
      const fixture = createContextFixture(client);
      const manager = new SilverPassManager(() => fixture.page);

      await manager.analyzeCurrentPage();

      assertCalls(
        client.calls,
        [
          "waitForReady",
          `isVisible:${SILVER_PASS_DEMO_MARKER}`,
          "clearEffects",
          `getScopedText:${SILVER_PASS_DEMO_MARKER}`,
          `isVisible:${TARGET_SELECTOR}`,
        ],
        "analysis should validate scope before reading and validate the target last",
      );
      assertEquals(
        manager.state().status,
        "ready",
        "valid content should be ready",
      );
      assert(
        manager.state().analysis !== null,
        "ready state should include the deterministic analysis",
      );
      assertEquals(
        manager.state().analysis?.terms.length,
        5,
        "ready analysis should expose all five terms",
      );
    },
  },
  {
    name: "null scoped text produces the specific retrieval error",
    async fn() {
      const client = new RecordingClient({
        getScopedText: () => Promise.resolve(null),
      });
      const fixture = createContextFixture(client);
      const manager = new SilverPassManager(() => fixture.page);

      await manager.analyzeCurrentPage();

      assertCalls(
        client.calls,
        [
          "waitForReady",
          `isVisible:${SILVER_PASS_DEMO_MARKER}`,
          "clearEffects",
          `getScopedText:${SILVER_PASS_DEMO_MARKER}`,
        ],
        "null text must stop before analysis and target lookup",
      );
      assertEquals(
        manager.state().status,
        "error",
        "null text should be an error",
      );
      assertEquals(
        manager.state().messageKey,
        "silverPassDemo.errorTextRetrievalFailed",
        "null text should use the retrieval-specific message",
      );
    },
  },
  {
    name: "readiness failure produces the specific not-ready error",
    async fn() {
      const client = new RecordingClient({
        waitForReady: () => Promise.resolve(false),
      });
      const fixture = createContextFixture(client);
      const manager = new SilverPassManager(() => fixture.page);

      await manager.analyzeCurrentPage();

      assertCalls(
        client.calls,
        ["waitForReady"],
        "a readiness failure must stop before reading page content",
      );
      assertEquals(
        manager.state().status,
        "error",
        "a readiness failure should be an error",
      );
      assertEquals(
        manager.state().messageKey,
        "silverPassDemo.errorNotReady",
        "a readiness failure should have a specific message",
      );
    },
  },
  {
    name: "a page query exception is contained as a generic panel error",
    async fn() {
      const client = new RecordingClient({
        getScopedText: () => Promise.reject(new Error("query failed")),
      });
      const fixture = createContextFixture(client);
      const manager = new SilverPassManager(() => fixture.page);

      await manager.analyzeCurrentPage();

      assertEquals(
        manager.state().status,
        "error",
        "a rejected query should be contained as an error state",
      );
      assertEquals(
        manager.state().messageKey,
        "silverPassDemo.errorGeneric",
        "an unexpected query failure should use the generic safe message",
      );
    },
  },
  {
    name: "an unavailable client produces an actor error without page calls",
    async fn() {
      const fixture = createContextFixture(null);
      const manager = new SilverPassManager(() => fixture.page);

      await manager.analyzeCurrentPage();

      assertEquals(
        fixture.createClientCount(),
        1,
        "the supported page should attempt actor acquisition once",
      );
      assertEquals(
        manager.state().status,
        "error",
        "missing actor should be an error",
      );
      assertEquals(
        manager.state().messageKey,
        "silverPassDemo.errorActorUnavailable",
        "missing actor should have a specific message",
      );
    },
  },
  {
    name:
      "highlight revalidates marker and target then clears before scrolling",
    async fn() {
      const client = new RecordingClient();
      const fixture = createContextFixture(client);
      const manager = new SilverPassManager(() => fixture.page);
      await manager.analyzeCurrentPage();
      client.calls.splice(0);

      const highlighted = await manager.highlightNextStep();

      assertEquals(highlighted, true, "a valid target should be highlighted");
      assertCalls(
        client.calls,
        [
          "waitForReady",
          `isVisible:${SILVER_PASS_DEMO_MARKER}`,
          `isVisible:${TARGET_SELECTOR}`,
          "clearEffects",
          `scrollToElement:${TARGET_SELECTOR}`,
        ],
        "highlighting should revalidate scope and clear old effects before scrolling",
      );
    },
  },
  {
    name: "highlight rejects a stale page before new actor acquisition",
    async fn() {
      let isCurrent = true;
      const client = new RecordingClient();
      const fixture = createContextFixture(client);
      fixture.page.isCurrent = () => isCurrent;
      const manager = new SilverPassManager(() => fixture.page);
      await manager.analyzeCurrentPage();
      const acquisitionCount = fixture.createClientCount();
      client.calls.splice(0);
      isCurrent = false;

      const highlighted = await manager.highlightNextStep();

      assertEquals(
        highlighted,
        false,
        "a stale page should cancel highlighting",
      );
      assertEquals(
        fixture.createClientCount(),
        acquisitionCount,
        "a stale page must not acquire another actor client",
      );
      assertCalls(
        client.calls,
        [],
        "a stale page must not send highlight queries",
      );
    },
  },
  {
    name: "highlight rejects a changed URL before acquiring a new client",
    async fn() {
      const client = new RecordingClient();
      const fixture = createContextFixture(client);
      const manager = new SilverPassManager(() => fixture.page);
      await manager.analyzeCurrentPage();
      const acquisitionCount = fixture.createClientCount();
      client.calls.splice(0);
      fixture.page.url = "https://example.com/after-navigation";

      const highlighted = await manager.highlightNextStep();

      assertEquals(
        highlighted,
        false,
        "navigation should invalidate the action",
      );
      assertEquals(
        fixture.createClientCount(),
        acquisitionCount,
        "the manager must reject the changed URL before actor acquisition",
      );
      assertCalls(
        client.calls,
        [],
        "the previous page client must not be reused",
      );
      assertEquals(
        manager.state().status,
        "unsupported",
        "a changed URL should become unsupported",
      );
    },
  },
  {
    name: "highlight stops when the marker disappears",
    async fn() {
      let markerVisible = true;
      const client = new RecordingClient({
        isVisible: (selector) =>
          Promise.resolve(
            selector === SILVER_PASS_DEMO_MARKER ? markerVisible : true,
          ),
      });
      const fixture = createContextFixture(client);
      const manager = new SilverPassManager(() => fixture.page);
      await manager.analyzeCurrentPage();
      client.calls.splice(0);
      markerVisible = false;

      const highlighted = await manager.highlightNextStep();

      assertEquals(
        highlighted,
        false,
        "a missing marker should cancel highlighting",
      );
      assertCalls(
        client.calls,
        ["waitForReady", `isVisible:${SILVER_PASS_DEMO_MARKER}`],
        "a missing marker must stop before target lookup or effects",
      );
    },
  },
  {
    name: "highlight stops when the target disappears",
    async fn() {
      let targetVisible = true;
      const client = new RecordingClient({
        isVisible: (selector) =>
          Promise.resolve(selector === TARGET_SELECTOR ? targetVisible : true),
      });
      const fixture = createContextFixture(client);
      const manager = new SilverPassManager(() => fixture.page);
      await manager.analyzeCurrentPage();
      client.calls.splice(0);
      targetVisible = false;

      const highlighted = await manager.highlightNextStep();

      assertEquals(
        highlighted,
        false,
        "a missing target should cancel highlighting",
      );
      assertCalls(
        client.calls,
        [
          "waitForReady",
          `isVisible:${SILVER_PASS_DEMO_MARKER}`,
          `isVisible:${TARGET_SELECTOR}`,
        ],
        "a missing target must stop before clearing or scrolling",
      );
      assertEquals(
        manager.state().status,
        "error",
        "missing target should be an error",
      );
    },
  },
  {
    name: "a fragment-only navigation does not cancel an in-flight analysis",
    async fn() {
      const deferredReady = createDeferred<boolean>();
      const client = new RecordingClient({
        waitForReady: () => deferredReady.promise,
      });
      const initialUrl = `${SILVER_PASS_DEMO_URL}#top`;
      let currentUrl = initialUrl;
      const page: SilverPassPageContext = {
        url: initialUrl,
        createClient: () => client,
        isCurrent: () => isSameSilverPassDocument(currentUrl, initialUrl),
      };
      const manager = new SilverPassManager(() => page);

      const analysis = manager.analyzeCurrentPage();
      currentUrl = `${SILVER_PASS_DEMO_URL}#overview`;
      deferredReady.resolve(true);
      await analysis;

      assertEquals(
        manager.state().status,
        "ready",
        "a same-document fragment change should not leave the panel loading",
      );
    },
  },
  {
    name: "a stale overlapping analysis cannot overwrite newer state",
    async fn() {
      const deferredReady = createDeferred<boolean>();
      const staleClient = new RecordingClient({
        waitForReady: () => deferredReady.promise,
      });
      const staleFixture = createContextFixture(staleClient);
      const newerFixture = createContextFixture(
        new RecordingClient(),
        "https://example.com/newer-page",
      );
      let providerCall = 0;
      const manager = new SilverPassManager(() => {
        providerCall += 1;
        return providerCall === 1 ? staleFixture.page : newerFixture.page;
      });

      const staleAnalysis = manager.analyzeCurrentPage();
      await manager.analyzeCurrentPage();
      assertEquals(
        manager.state().status,
        "unsupported",
        "the newer unsupported result should win immediately",
      );
      deferredReady.resolve(true);
      await staleAnalysis;

      assertEquals(
        manager.state().status,
        "unsupported",
        "the older request must not replace the newer result",
      );
      assertCalls(
        staleClient.calls,
        ["waitForReady"],
        "the stale request should stop at its first current-operation check",
      );
      assertEquals(
        newerFixture.createClientCount(),
        0,
        "the newer unsupported URL should not acquire a client",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("silverPassManager.test.ts", tests);
}
