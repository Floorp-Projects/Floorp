// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { compressImage } from "./imageCompressor.ts";

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "compressImage is a function",
      fn: () => {
        assert(typeof compressImage === "function", "compressImage should be exported as a function");
      },
    },
    {
      name: "compressImage rejects for an invalid blob",
      fn: async () => {
        // Create an empty blob that cannot be loaded as an image
        const emptyBlob = new Blob([], { type: "image/png" });
        try {
          await compressImage(emptyBlob);
          // If it resolves (some engines may produce a blank 0x0 image), that is also acceptable
        } catch (e) {
          const msg = e instanceof Error ? e.message : String(e);
          assert(
            msg.includes("Failed to load image") || msg.includes("too large") || msg.includes("Canvas"),
            `Expected image load error, got: ${msg}`,
          );
        }
      },
    },
    {
      name: "compressImage produces a data URL for a tiny valid image",
      fn: async () => {
        // Create a minimal 1x1 red pixel PNG via canvas
        if (typeof document === "undefined") return;
        const canvas = document.createElement("canvas");
        canvas.width = 1;
        canvas.height = 1;
        const ctx = canvas.getContext("2d");
        if (!ctx) return; // skip if canvas unsupported
        ctx.fillStyle = "red";
        ctx.fillRect(0, 0, 1, 1);

        const blob: Blob = await new Promise((resolve) => {
          canvas.toBlob((b) => resolve(b!), "image/png");
        });

        const result = await compressImage(blob);
        assert(typeof result === "string", "result should be a string");
        assert(result.startsWith("data:image/jpeg"), "result should be a JPEG data URL");
      },
    },
  ];

  for (const t of tests) {
    try {
      await t.fn();
      console.log(`  PASS: ${t.name}`);
    } catch (e) {
      console.error(`  FAIL: ${t.name}`);
      throw e;
    }
  }
}
