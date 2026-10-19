// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  downloadChromeExtension,
  getChromeDownloadTarget,
} from "../ChromeExtensionUpdater.sys.mts";
import {
  CRX_ACCEPT_HEADER,
  CRX_DOWNLOAD_URLS,
  MAX_CRX_SIZE,
} from "../Constants.sys.mts";
import { CRX_TEST_ID } from "./helpers/CRXFixtures.mts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

async function withFetch(
  replacement: typeof fetch,
  fn: () => Promise<void>,
): Promise<void> {
  const original = globalThis.fetch;
  globalThis.fetch = (input, options) => {
    if (
      String(input).startsWith(
        "https://clients2.google.com/service/update2/crx?",
      )
    ) {
      return replacement(input, options);
    }
    return original(input, options);
  };
  try {
    await fn();
  } finally {
    globalThis.fetch = original;
  }
}

async function rejected(
  promise: Promise<unknown>,
  reason: string,
): Promise<void> {
  try {
    await promise;
  } catch (error) {
    assert(
      String(error).includes(reason),
      `failure explains ${reason}: ${error}`,
    );
    return;
  }
  throw new Error(`Expected rejection describing ${reason}`);
}

const tests: TestCase[] = [
  {
    name: "invalid original IDs are rejected before any network request",
    async fn() {
      let requests = 0;
      await withFetch(
        async () => {
          requests++;
          return new Response(new Uint8Array([1]));
        },
        async () => {
          for (
            const id of [
              "",
              "z".repeat(32),
              `${CRX_TEST_ID}@chromium-extension`,
              `${CRX_TEST_ID}&other=1`,
            ]
          ) {
            await rejected(
              downloadChromeExtension(id),
              "Invalid Chrome extension ID",
            );
          }
        },
      );
      assertEquals(requests, 0, "invalid IDs never reach either endpoint");
    },
  },
  {
    name: "streamed download omits credentials and preserves byte order",
    async fn() {
      let requests = 0;
      await withFetch(
        async (input, options) => {
          requests++;
          assertEquals(
            String(input),
            CRX_DOWNLOAD_URLS.primary(CRX_TEST_ID, getChromeDownloadTarget()),
            "primary official endpoint",
          );
          assertEquals(
            options?.credentials,
            "omit",
            "profile credentials are excluded",
          );
          assertEquals(
            new Headers(options?.headers).get("Accept"),
            CRX_ACCEPT_HEADER,
            "CRX MIME type requested",
          );
          assert(
            options?.signal instanceof AbortSignal,
            "download has an abortable deadline",
          );
          const parts = [
            new Uint8Array([0x43, 0x72]),
            new Uint8Array([0x32, 0x34, 3, 0, 0, 0]),
          ];
          return new Response(
            new ReadableStream<Uint8Array>({
              pull(controller) {
                const part = parts.shift();
                if (part) controller.enqueue(part);
                else controller.close();
              },
            }),
          );
        },
        async () => {
          const result = new Uint8Array(
            await downloadChromeExtension(CRX_TEST_ID),
          );
          assertEquals(
            Array.from(result).join(),
            "67,114,50,52,3,0,0,0",
            "all streamed chunks concatenated in order",
          );
        },
      );
      assertEquals(requests, 1, "successful primary does not request fallback");
    },
  },
  {
    name:
      "oversized declared downloads cancel both response bodies before reading",
    async fn() {
      let requests = 0;
      let cancelled = 0;
      let pulled = 0;
      await withFetch(
        async () => {
          requests++;
          return new Response(
            new ReadableStream<Uint8Array>(
              {
                pull() {
                  pulled++;
                },
                cancel() {
                  cancelled++;
                },
              },
              { highWaterMark: 0 },
            ),
            {
              headers: { "Content-Length": String(MAX_CRX_SIZE + 1) },
            },
          );
        },
        async () => {
          await rejected(
            downloadChromeExtension(CRX_TEST_ID),
            "package size limit",
          );
        },
      );
      assertEquals(requests, 2, "primary and fallback are both bounded");
      assertEquals(cancelled, 2, "oversized response bodies released");
      assertEquals(pulled, 0, "oversized body is never read");
    },
  },
  {
    name:
      "streamed size limit overrides an understated Content-Length and cancels the reader",
    async fn() {
      let requests = 0;
      let cancelled = 0;
      let pulled = 0;
      // Reuse the same chunk so the test crosses the logical download bound
      // without allocating hundreds of megabytes of distinct payload data.
      const chunk = new Uint8Array(16 * 1024 * 1024);
      await withFetch(
        async () => {
          requests++;
          return new Response(
            new ReadableStream<Uint8Array>(
              {
                pull(controller) {
                  pulled++;
                  controller.enqueue(chunk);
                },
                cancel() {
                  cancelled++;
                },
              },
              { highWaterMark: 0 },
            ),
            { headers: { "Content-Length": "1" } },
          );
        },
        async () => {
          await rejected(
            downloadChromeExtension(CRX_TEST_ID),
            "package size limit",
          );
        },
      );
      assertEquals(
        requests,
        2,
        "both endpoint streams enforce actual byte count",
      );
      assertEquals(cancelled, 2, "over-limit streams cancelled");
      assertEquals(
        pulled,
        2 * (Math.floor(MAX_CRX_SIZE / chunk.length) + 1),
        "reading stops at the first oversized chunk",
      );
    },
  },
  {
    name:
      "HTTP failure releases its body and falls back without profile credentials",
    async fn() {
      const urls: string[] = [];
      let cancelled = 0;
      await withFetch(
        async (input, options) => {
          urls.push(String(input));
          assertEquals(
            options?.credentials,
            "omit",
            "fallback also excludes credentials",
          );
          if (urls.length === 1) {
            return new Response(
              new ReadableStream<Uint8Array>(
                {
                  cancel() {
                    cancelled++;
                  },
                },
                { highWaterMark: 0 },
              ),
              { status: 503 },
            );
          }
          return new Response(new Uint8Array([17, 23]));
        },
        async () => {
          const result = await downloadChromeExtension(CRX_TEST_ID);
          assertEquals(
            Array.from(new Uint8Array(result)).join(),
            "17,23",
            "fallback payload returned",
          );
        },
      );
      assertEquals(
        urls.join("\n"),
        [
          CRX_DOWNLOAD_URLS.primary(CRX_TEST_ID, getChromeDownloadTarget()),
          CRX_DOWNLOAD_URLS.fallback(CRX_TEST_ID, getChromeDownloadTarget()),
        ].join("\n"),
        "fallback uses the fixed official endpoint",
      );
      assertEquals(cancelled, 1, "unused failing response body cancelled");
    },
  },
  {
    name:
      "network failures exhaust only the two known endpoints and propagate the failure",
    async fn() {
      let attempts = 0;
      await withFetch(
        async () => {
          attempts++;
          throw new Error(`fixture network failure ${attempts}`);
        },
        async () => {
          await rejected(
            downloadChromeExtension(CRX_TEST_ID),
            "fixture network failure 2",
          );
        },
      );
      assertEquals(attempts, 2, "bounded endpoint attempts");
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("ChromeExtensionDownload.test.mts", tests);
}
