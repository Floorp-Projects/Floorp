// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals as browserAssertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import { HighlightManager } from "../HighlightManager.ts";

function assertEquals<T>(actual: T, expected: T): void {
  browserAssertEquals(actual, expected, `expected ${expected}, got ${actual}`);
}

function fixture(value = "", priority = "") {
  const doc = document.implementation.createHTMLDocument(
    "control overlay test",
  );
  doc.body.style.setProperty("pointer-events", value, priority);
  const manager = new HighlightManager({
    document: doc,
    contentWindow: window,
    sendQuery: () => Promise.resolve(true),
  });
  // These tests exercise real inline CSS declarations, without adding shared
  // styles or relying on layout in a document without a browsing context.
  manager.ensureHighlightStyle = () => {};
  manager.showControlOverlay();
  const overlay = doc.getElementById("nr-webscraper-control-overlay");
  const label = doc.querySelector<HTMLElement>(
    ".nr-webscraper-control-overlay__label",
  );
  assert(overlay && label, "control UI should be present");
  return { doc, manager, overlay: overlay as HTMLElement, label };
}

const tests: TestCase[] = [
  {
    name: "native suspension is synchronous and restores nested calls",
    fn() {
      const f = fixture("auto", "important");
      try {
        assertEquals(
          f.doc.body.style.getPropertyValue("pointer-events"),
          "none",
        );
        const returned = f.manager.withControlOverlaySuspended(() => {
          assertEquals(
            f.doc.body.style.getPropertyValue("pointer-events"),
            "auto",
          );
          assertEquals(
            f.doc.body.style.getPropertyPriority("pointer-events"),
            "important",
          );
          assertEquals(
            f.overlay.style.getPropertyValue("pointer-events"),
            "none",
          );
          assertEquals(
            f.label.style.getPropertyValue("pointer-events"),
            "none",
          );
          f.manager.withControlOverlaySuspended(() => {
            assertEquals(
              f.doc.body.style.getPropertyValue("pointer-events"),
              "auto",
            );
          });
          assertEquals(
            f.doc.body.style.getPropertyValue("pointer-events"),
            "auto",
          );
          return 42;
        });
        assertEquals(returned, 42);
        assertEquals(
          f.doc.body.style.getPropertyValue("pointer-events"),
          "none",
        );
        assertEquals(f.overlay.style.getPropertyValue("pointer-events"), "");
        assertEquals(f.label.style.getPropertyValue("pointer-events"), "");
        f.manager.hideControlOverlay();
        assertEquals(
          f.doc.body.style.getPropertyValue("pointer-events"),
          "auto",
        );
        assertEquals(
          f.doc.body.style.getPropertyPriority("pointer-events"),
          "important",
        );
      } finally {
        f.manager.destroy();
      }
    },
  },
  {
    name: "native delivery exceptions restore control blocking",
    fn() {
      const f = fixture();
      try {
        let caught = false;
        try {
          f.manager.withControlOverlaySuspended(() => {
            throw new Error("delivery failed");
          });
        } catch {
          caught = true;
        }
        assert(caught, "original exception should propagate");
        assertEquals(
          f.doc.body.style.getPropertyValue("pointer-events"),
          "none",
        );
        assertEquals(
          f.doc.body.style.getPropertyPriority("pointer-events"),
          "important",
        );
        assertEquals(f.overlay.style.getPropertyValue("pointer-events"), "");
      } finally {
        f.manager.destroy();
      }
      assertEquals(f.doc.body.style.getPropertyValue("pointer-events"), "");
    },
  },
  {
    name: "suspension keeps website pointer guards and overlays intact",
    fn() {
      const f = fixture("none", "important");
      const siteOverlay = f.doc.createElement("div");
      siteOverlay.style.setProperty("pointer-events", "all", "important");
      f.doc.body.appendChild(siteOverlay);
      try {
        f.manager.withControlOverlaySuspended(() => {
          assertEquals(
            f.doc.body.style.getPropertyValue("pointer-events"),
            "none",
          );
          assertEquals(
            f.doc.body.style.getPropertyPriority("pointer-events"),
            "important",
          );
          assertEquals(
            siteOverlay.style.getPropertyValue("pointer-events"),
            "all",
          );
          assertEquals(
            siteOverlay.style.getPropertyPriority("pointer-events"),
            "important",
          );
        });
      } finally {
        f.manager.destroy();
      }
      assertEquals(f.doc.body.style.getPropertyValue("pointer-events"), "none");
      assertEquals(
        f.doc.body.style.getPropertyPriority("pointer-events"),
        "important",
      );
    },
  },
  {
    name: "cleanup during native dispatch does not reinstate blocking",
    fn() {
      const f = fixture("auto");
      try {
        f.manager.withControlOverlaySuspended(() => {
          f.manager.hideControlOverlay();
        });
        assertEquals(
          f.doc.body.style.getPropertyValue("pointer-events"),
          "auto",
        );
        assertEquals(
          f.overlay.style.getPropertyValue("pointer-events"),
          "none",
        );
      } finally {
        f.manager.destroy();
      }
    },
  },
  {
    name: "website style changes during dispatch survive later cleanup",
    fn() {
      const f = fixture();
      try {
        f.manager.withControlOverlaySuspended(() => {
          f.doc.body.style.setProperty("pointer-events", "auto", "important");
        });
        assertEquals(
          f.doc.body.style.getPropertyValue("pointer-events"),
          "none",
        );
      } finally {
        f.manager.destroy();
      }
      assertEquals(f.doc.body.style.getPropertyValue("pointer-events"), "auto");
      assertEquals(
        f.doc.body.style.getPropertyPriority("pointer-events"),
        "important",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("HighlightManager.test.ts", tests);
}
