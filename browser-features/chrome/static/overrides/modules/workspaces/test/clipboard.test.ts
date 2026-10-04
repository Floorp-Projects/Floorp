// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { readNewTabClipboard } from "../clipboard.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../test/utils/test_harness.ts";

const fail = (): never => {
  throw new Error("Clipboard unavailable");
};
const identity = (text: string) => text;

export async function runAllTests() {
  await runTests("new tab clipboard", [
    {
      name: "Prefers the selection without reading the global clipboard",
      fn: () =>
        assertEquals(
          readNewTabClipboard(() => " https://example.com/ ", fail, identity),
          "https://example.com/",
          "Selection wins",
        ),
    },
    {
      name: "Falls back for empty, whitespace-only and unavailable selections",
      fn: () => {
        for (const selection of [() => "", () => " \n\t ", fail]) {
          assertEquals(
            readNewTabClipboard(
              selection,
              () => " https://example.org/ ",
              identity,
            ),
            "https://example.org/",
            "Global clipboard fallback",
          );
        }
      },
    },
    {
      name: "Returns an empty value when both clipboard reads fail",
      fn: () =>
        assertEquals(
          readNewTabClipboard(fail, fail, identity),
          "",
          "Failed reads are ignored",
        ),
    },
    {
      name: "Rejects clipboard text when sanitization is missing or fails",
      fn: () => {
        for (const sanitize of [undefined, fail]) {
          assertEquals(
            readNewTabClipboard(() => "javascript:alert(1)", fail, sanitize),
            "",
            "Unsanitized text is rejected",
          );
          assertEquals(
            readNewTabClipboard(
              () => "",
              () => "javascript:alert(1)",
              sanitize,
            ),
            "",
            "Unsanitized text is rejected",
          );
        }
      },
    },
    {
      name: "Uses the native URL sanitizer for both clipboard sources",
      fn: () => {
        const sanitize = globalThis.UrlbarShared?.stripUnsafeProtocolOnPaste ??
          globalThis.UrlbarUtils?.stripUnsafeProtocolOnPaste;
        assert(
          typeof sanitize === "function",
          "Native URL sanitizer must be available",
        );
        for (
          const text of [
            "javascript:alert(1)",
            "javascript:javascript:alert(1)",
            "https://example.com/",
          ]
        ) {
          const expected = sanitize(text).trim();
          assert(
            !expected.toLowerCase().startsWith("javascript:"),
            "Unsafe protocol survived native sanitization",
          );
          assertEquals(
            readNewTabClipboard(() => text, fail, sanitize),
            expected,
            "Native sanitization is preserved",
          );
          assertEquals(
            readNewTabClipboard(() => " ", () => text, sanitize),
            expected,
            "Native sanitization is preserved",
          );
        }
      },
    },
  ]);
}
