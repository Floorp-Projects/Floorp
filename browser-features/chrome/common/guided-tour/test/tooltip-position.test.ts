// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  computeTooltipPosition,
  getTooltipRect,
  rectsIntersect,
  resolveTooltipPosition,
  SPOTLIGHT_PADDING,
  TOOLTIP_GAP,
} from "../tooltip-position.ts";
import type { TargetRect } from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testRectsIntersect(): void {
  const a: TargetRect = { x: 0, y: 0, width: 100, height: 100 };
  const b: TargetRect = { x: 50, y: 50, width: 100, height: 100 };
  const c: TargetRect = { x: 200, y: 200, width: 10, height: 10 };
  assert(rectsIntersect(a, b), "overlapping rects should intersect");
  assert(!rectsIntersect(a, c), "disjoint rects should not intersect");
}

function testNoObstaclesMatchesCompute(): void {
  const spotlight: TargetRect = { x: 100, y: 200, width: 80, height: 40 };
  const w = 320;
  const h = 160;
  const vw = 1280;
  const vh = 800;
  const expected = computeTooltipPosition(
    spotlight,
    "bottom",
    w,
    h,
    vw,
    vh,
  );
  const resolved = resolveTooltipPosition(
    spotlight,
    "bottom",
    w,
    h,
    vw,
    vh,
    [],
  );
  assertEquals(
    resolved.left,
    expected.left,
    "left should match without obstacles",
  );
  assertEquals(
    resolved.top,
    expected.top,
    "top should match without obstacles",
  );
}

function testWorkspacesButtonAvoidsUrlbar(): void {
  // 調査時の実測値に基づくフィクスチャ
  const button: TargetRect = { x: 1069, y: 0, width: 186, height: 36 };
  const spotlight: TargetRect = {
    x: button.x - SPOTLIGHT_PADDING,
    y: button.y - SPOTLIGHT_PADDING,
    width: button.width + SPOTLIGHT_PADDING * 2,
    height: button.height + SPOTLIGHT_PADDING * 2,
  };
  const urlbar: TargetRect = { x: 119, y: 41, width: 970, height: 30 };
  const toolbox: TargetRect = { x: 0, y: 0, width: 1280, height: 76 };
  const w = 320;
  const h = 221;
  const vw = 1280;
  const vh = 800;

  const pos = resolveTooltipPosition(
    spotlight,
    "bottom",
    w,
    h,
    vw,
    vh,
    [urlbar, toolbox],
  );
  const tooltipRect = getTooltipRect(pos, w, h);

  assert(
    !rectsIntersect(tooltipRect, urlbar),
    `tooltip should not intersect urlbar (top=${pos.top})`,
  );
  assert(
    !rectsIntersect(tooltipRect, toolbox),
    `tooltip should not intersect navigator-toolbox (top=${pos.top})`,
  );
  assert(
    pos.top >= toolbox.y + toolbox.height + TOOLTIP_GAP - 1,
    `tooltip should sit below toolbox (top=${pos.top}, toolboxBottom=${
      toolbox.y + toolbox.height
    })`,
  );
}

function testFallbackShiftsBelowObstacle(): void {
  const spotlight: TargetRect = { x: 1063, y: -6, width: 198, height: 48 };
  const urlbar: TargetRect = { x: 119, y: 41, width: 970, height: 30 };
  const w = 320;
  const h = 221;
  const vw = 1280;
  const vh = 800;

  const pos = resolveTooltipPosition(
    spotlight,
    "left",
    w,
    h,
    vw,
    vh,
    [urlbar],
  );
  const tooltipRect = getTooltipRect(pos, w, h);

  assert(
    !rectsIntersect(tooltipRect, urlbar),
    "fallback position should clear urlbar",
  );
  assert(
    pos.top > urlbar.y + urlbar.height,
    `tooltip top (${pos.top}) should be below urlbar bottom (${
      urlbar.y + urlbar.height
    })`,
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "rectsIntersect", fn: testRectsIntersect },
    {
      name: "no obstacles matches computeTooltipPosition",
      fn: testNoObstaclesMatchesCompute,
    },
    {
      name: "workspaces button avoids urlbar",
      fn: testWorkspacesButtonAvoidsUrlbar,
    },
    {
      name: "fallback shifts below obstacle",
      fn: testFallbackShiftsBelowObstacle,
    },
  ];

  await runTests("guided-tour/tooltip-position.test.ts", tests);
}
