// SPDX-License-Identifier: MPL-2.0

import type { TargetRect, TooltipPlacement } from "./types.ts";

export const SPOTLIGHT_PADDING = 6;
export const TOOLTIP_GAP = 12;
const VIEWPORT_MARGIN = 8;

const PLACEMENT_ORDER: TooltipPlacement[] = [
  "bottom",
  "top",
  "left",
  "right",
  "center",
];

export interface TooltipPosition {
  left: number;
  top: number;
}

export function rectsIntersect(a: TargetRect, b: TargetRect): boolean {
  return (
    a.x < b.x + b.width &&
    a.x + a.width > b.x &&
    a.y < b.y + b.height &&
    a.y + a.height > b.y
  );
}

export function getTooltipRect(
  pos: TooltipPosition,
  width: number,
  height: number,
): TargetRect {
  return { x: pos.left, y: pos.top, width, height };
}

export function computeTooltipPosition(
  rect: TargetRect | null,
  placement: TooltipPlacement,
  tooltipW: number,
  tooltipH: number,
  viewportW: number,
  viewportH: number,
): TooltipPosition {
  if (!rect || placement === "center") {
    return {
      left: (viewportW - tooltipW) / 2,
      top: (viewportH - tooltipH) / 2,
    };
  }

  const pad = SPOTLIGHT_PADDING + TOOLTIP_GAP;
  const centerX = rect.x + rect.width / 2;
  const centerY = rect.y + rect.height / 2;

  const positions: Record<
    Exclude<TooltipPlacement, "center">,
    TooltipPosition
  > = {
    top: { left: centerX - tooltipW / 2, top: rect.y - pad - tooltipH },
    bottom: { left: centerX - tooltipW / 2, top: rect.y + rect.height + pad },
    left: { left: rect.x - pad - tooltipW, top: centerY - tooltipH / 2 },
    right: { left: rect.x + rect.width + pad, top: centerY - tooltipH / 2 },
  };

  const fits = (p: TooltipPosition) =>
    p.left >= VIEWPORT_MARGIN &&
    p.top >= VIEWPORT_MARGIN &&
    p.left + tooltipW <= viewportW - VIEWPORT_MARGIN &&
    p.top + tooltipH <= viewportH - VIEWPORT_MARGIN;

  const flipOf: Record<string, TooltipPlacement> = {
    top: "bottom",
    bottom: "top",
    left: "right",
    right: "left",
  };

  let pos = positions[placement];
  if (!fits(pos)) {
    const flipped = positions[
      flipOf[placement] as Exclude<TooltipPlacement, "center">
    ];
    if (fits(flipped)) pos = flipped;
  }

  return {
    left: Math.min(
      Math.max(pos.left, VIEWPORT_MARGIN),
      viewportW - tooltipW - VIEWPORT_MARGIN,
    ),
    top: Math.min(
      Math.max(pos.top, VIEWPORT_MARGIN),
      viewportH - tooltipH - VIEWPORT_MARGIN,
    ),
  };
}

function intersectsAnyObstacle(
  tooltipRect: TargetRect,
  obstacles: TargetRect[],
): boolean {
  return obstacles.some((obstacle) => rectsIntersect(tooltipRect, obstacle));
}

/**
 * 障害物矩形と交差しないツールチップ位置を返す。
 * すべての placement で交差する場合は最大 obstacle.bottom より下へシフトする。
 */
export function resolveTooltipPosition(
  rect: TargetRect | null,
  preferredPlacement: TooltipPlacement,
  tooltipW: number,
  tooltipH: number,
  viewportW: number,
  viewportH: number,
  obstacles: TargetRect[],
): TooltipPosition {
  if (obstacles.length === 0) {
    return computeTooltipPosition(
      rect,
      preferredPlacement,
      tooltipW,
      tooltipH,
      viewportW,
      viewportH,
    );
  }

  // ターゲットがあるときは center を試さない（ボタンから離れすぎるため）
  const alternatives = rect === null
    ? PLACEMENT_ORDER
    : PLACEMENT_ORDER.filter((p) => p !== "center");
  const placements = [
    preferredPlacement,
    ...alternatives.filter((p) => p !== preferredPlacement),
  ];

  for (const placement of placements) {
    const pos = computeTooltipPosition(
      rect,
      placement,
      tooltipW,
      tooltipH,
      viewportW,
      viewportH,
    );
    const tooltipRect = getTooltipRect(pos, tooltipW, tooltipH);
    if (!intersectsAnyObstacle(tooltipRect, obstacles)) {
      return pos;
    }
  }

  const basePos = computeTooltipPosition(
    rect,
    preferredPlacement,
    tooltipW,
    tooltipH,
    viewportW,
    viewportH,
  );
  const maxBottom = Math.max(
    ...obstacles.map((obstacle) => obstacle.y + obstacle.height),
  );
  return {
    left: basePos.left,
    top: Math.min(
      Math.max(maxBottom + TOOLTIP_GAP, VIEWPORT_MARGIN),
      viewportH - tooltipH - VIEWPORT_MARGIN,
    ),
  };
}

/** Chrome UI 要素の矩形を障害物として収集する（Popover Top Layer の urlbar 等） */
export function getChromeObstacleRects(): TargetRect[] {
  const obstacles: TargetRect[] = [];
  for (const id of ["urlbar", "navigator-toolbox"]) {
    const el = document.getElementById(id);
    if (!el) continue;
    const r = el.getBoundingClientRect();
    if (r.width > 0 && r.height > 0) {
      obstacles.push({
        x: r.x,
        y: r.y,
        width: r.width,
        height: r.height,
      });
    }
  }
  return obstacles;
}
