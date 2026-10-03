// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Issue #2816. These tests require the platform-condition fix in PR #2819.

import {
  buildChromeExtrasCSS,
  CHROME_EXTRAS_DEFAULTS,
} from "../chrome-extras.ts";
import { assert, runTests } from "../../../test/utils/test_harness.ts";
import type { MenuIconPopup } from "./types.ts";

async function testMenuGeometry(): Promise<void> {
  // Native Cocoa menus do not use DOM layout; this geometry test targets the
  // Windows/Linux platforms reported in #2816. Run with both OS CI runners.
  if (Services.appinfo.OS === "Darwin") return;
  const style = document.createElement("style");
  style.textContent = buildChromeExtrasCSS(
    { ...CHROME_EXTRAS_DEFAULTS, iconMenu: true },
    "lepton",
  );
  const set = document.createXULElement("popupset");
  const popup = document.createXULElement("menupopup") as MenuIconPopup;
  popup.setAttribute("nonnative", "true");
  // Pin the image edge while retaining the production rules for text/gutter.
  popup.setAttribute("style", "--context-menu-background-padding: 6px;");
  const plain = document.createXULElement("menuitem");
  plain.setAttribute("label", "Plain item with a background icon");
  plain.setAttribute(
    "style",
    "--menuitem-image: url(chrome://browser/skin/bookmark-hollow.svg);",
  );
  popup.appendChild(plain);
  const bookmark = document.createXULElement("menuitem");
  bookmark.setAttribute("class", "menuitem-iconic bookmark-item");
  bookmark.setAttribute("image", "chrome://browser/skin/bookmark-hollow.svg");
  bookmark.setAttribute("label", "Bookmark with a real image");
  popup.appendChild(bookmark);
  const checks: Element[] = [];
  for (const type of ["checkbox", "radio"]) {
    const item = document.createXULElement("menuitem");
    item.setAttribute("type", type);
    item.setAttribute("checked", "true");
    item.setAttribute("label", `Checked ${type}`);
    popup.appendChild(item);
    checks.push(item);
  }
  set.appendChild(popup);
  document.documentElement.appendChild(set);
  document.head.appendChild(style);
  try {
    popup.openPopup(
      document.getElementById("nav-bar"),
      "after_start",
      0,
      0,
      false,
      false,
    );
    const deadline = Date.now() + 5000;
    while (popup.state !== "open") {
      assert(Date.now() < deadline, "test popup must open");
      await new Promise((resolve) => setTimeout(resolve, 25));
    }
    const label = plain.querySelector(".menu-text");
    assert(label, "Firefox must render the current .menu-text markup");
    const itemRect = plain.getBoundingClientRect();
    const labelRect = label.getBoundingClientRect();
    const computedStyle = getComputedStyle(plain);
    assert(computedStyle, "the open menu item must expose computed styles");
    const rtl = computedStyle.direction === "rtl";
    const textStart = rtl
      ? itemRect.right - labelRect.right
      : labelRect.left - itemRect.left;
    assert(
      textStart >= 24,
      `plain label must clear the 6px + 16px background icon (actual ${textStart})`,
    );

    const icon = bookmark.querySelector(".menu-icon");
    const bookmarkLabel = bookmark.querySelector(".menu-text");
    assert(
      icon && bookmarkLabel,
      "Firefox 157 uses .menu-icon and .menu-text for iconic bookmarks",
    );
    const iconRect = icon.getBoundingClientRect();
    const bookmarkTextRect = bookmarkLabel.getBoundingClientRect();
    assert(iconRect.width > 0, "bookmark image must stay visible");
    const gap = rtl
      ? iconRect.left - bookmarkTextRect.right
      : bookmarkTextRect.left - iconRect.right;
    assert(
      gap >= 2 && gap <= 16,
      `bookmark icon/text gap must not overlap or inherit macOS text padding (actual ${gap})`,
    );
    for (const check of checks) {
      const checkIcon = check.querySelector(".menu-icon");
      const checkText = check.querySelector(".menu-text");
      assert(
        checkIcon && checkText,
        "checked items must retain their native icon and label",
      );
      const mark = checkIcon.getBoundingClientRect();
      const text = checkText.getBoundingClientRect();
      assert(mark.width > 0, "checked item must retain its gutter");
      assert(
        rtl ? text.right <= mark.left : text.left >= mark.right,
        "checkbox/radio mark must not overlap the label",
      );
    }
  } finally {
    popup.hidePopup();
    set.remove();
    style.remove();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("menu-icons.test.ts", [
    {
      name: "menu icons and native checks clear their labels",
      fn: testMenuGeometry,
    },
  ]);
}

await runAllTests();
