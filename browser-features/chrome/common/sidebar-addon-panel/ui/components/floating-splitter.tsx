/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import { panelSidebarConfig, setIsFloatingDragging } from "../../core/data.ts";

export const [isResizeCooldown, setIsResizeCooldown] =
  createSignal<boolean>(false);
let resizeCooldownTimer: number | null = null;

export function FloatingSplitter() {
  const broadcastResize = (w: number, h: number) => {
    const doc = typeof document !== "undefined" ? document : null;
    if (!doc) return;
    try {
      const panels = doc.querySelectorAll(
        '[id^="sidebar-panel-"]',
      ) as NodeListOf<Element>;
      panels.forEach((p: Element) => {
        try {
          const browserLike = p as unknown as {
            browsingContext?: { associatedWindow?: Window };
          };
          const win = browserLike?.browsingContext?.associatedWindow;
          if (win && typeof win.postMessage === "function") {
            win.postMessage(
              {
                type: "panel-sidebar-resize",
                width: Math.round(w),
                height: Math.round(h),
              },
              "*",
            );
          }
        } catch {
          // ignore per-panel errors
        }
      });
    } catch {
      // ignore
    }
  };

  const onHorizontalMouseDown = (e: MouseEvent) => {
    setIsFloatingDragging(true);
    const sidebarBox = document?.getElementById(
      "panel-sidebar-box",
    ) as XULElement;
    const docEl = document?.documentElement as XULElement | null;

    if (!sidebarBox) {
      return;
    }

    const startX = e.clientX;
    const startWidth = sidebarBox.getBoundingClientRect().width;
    // left を offsetParent 座標系で取得
    const parentEl = (sidebarBox as unknown as HTMLElement)
      .offsetParent as HTMLElement | null;
    const parentRect = (
      parentEl ?? (document?.getElementById("browser") as HTMLElement)
    ).getBoundingClientRect();
    const boxRect = sidebarBox.getBoundingClientRect();
    const styleLeft = Number.parseInt(
      sidebarBox.style.getPropertyValue("left") || "NaN",
      10,
    );
    const startLeft = Number.isNaN(styleLeft)
      ? boxRect.left - parentRect.left
      : styleLeft;
    const isLeftSide = (e.target as HTMLElement).classList.contains(
      "floating-splitter-left",
    );

    let frameRequested = false;
    let pendingWidth = startWidth;
    let pendingLeft = startLeft;

    const applyFrame = () => {
      frameRequested = false;
      sidebarBox.style.setProperty("width", `${pendingWidth}px`);
      if (isLeftSide) {
        sidebarBox.style.setProperty("left", `${pendingLeft}px`);
      }
      try {
        const currH = sidebarBox.getBoundingClientRect().height;
        broadcastResize(pendingWidth, currH);
      } catch {
        /* noop */
      }
    };

    const onMouseMove = (e: MouseEvent) => {
      const deltaX = e.clientX - startX;
      const browserW =
        (document?.getElementById("browser") as HTMLElement | null)
          ?.clientWidth ?? window.innerWidth;
      const minW = 225;
      const maxW = browserW * 0.8;
      if (isLeftSide) {
        const rightEdge = startLeft + startWidth;
        const desiredWidth = startWidth - deltaX;
        pendingWidth = Math.max(minW, Math.min(desiredWidth, maxW));
        pendingLeft = Math.max(
          0,
          Math.min(browserW - pendingWidth, rightEdge - pendingWidth),
        );
      } else {
        const desiredWidth = startWidth + deltaX;
        pendingWidth = Math.max(minW, Math.min(desiredWidth, maxW));
      }
      if (!frameRequested) {
        frameRequested = true;
        document?.defaultView?.requestAnimationFrame(applyFrame);
      }
    };

    const onMouseUp = () => {
      setIsFloatingDragging(false);
      document?.removeEventListener("mousemove", onMouseMove);
      document?.removeEventListener("mouseup", onMouseUp);
      docEl?.style.removeProperty("user-select");

      setIsResizeCooldown(true);

      if (resizeCooldownTimer !== null) {
        clearTimeout(resizeCooldownTimer);
      }

      resizeCooldownTimer = globalThis.setTimeout(() => {
        setIsResizeCooldown(false);
        resizeCooldownTimer = null;
      }, 1000);
    };

    docEl?.style.setProperty("user-select", "none");
    document?.addEventListener("mousemove", onMouseMove);
    document?.addEventListener("mouseup", onMouseUp);
  };

  const onVerticalMouseDown = (e: MouseEvent) => {
    setIsFloatingDragging(true);
    const sidebarBox = document?.getElementById(
      "panel-sidebar-box",
    ) as XULElement;
    const docEl = document?.documentElement as XULElement | null;

    if (!sidebarBox) {
      return;
    }

    const startY = e.clientY;
    const startHeight = sidebarBox.getBoundingClientRect().height;
    // top を offsetParent 座標系で取得
    const parentEl = (sidebarBox as unknown as HTMLElement)
      .offsetParent as HTMLElement | null;
    const parentRect = (
      parentEl ?? (document?.getElementById("browser") as HTMLElement)
    ).getBoundingClientRect();
    const boxRect = sidebarBox.getBoundingClientRect();
    const styleTop = Number.parseInt(
      sidebarBox.style.getPropertyValue("top") || "NaN",
      10,
    );
    const startTop = Number.isNaN(styleTop)
      ? boxRect.top - parentRect.top
      : styleTop;
    const isTopSide = (e.target as HTMLElement).classList.contains(
      "floating-splitter-top",
    );

    let frameRequested = false;
    let pendingHeight = startHeight;
    let pendingTop = startTop;

    const applyFrame = () => {
      frameRequested = false;
      sidebarBox.style.setProperty("height", `${pendingHeight}px`);
      if (isTopSide) {
        sidebarBox.style.setProperty("top", `${pendingTop}px`);
      }
      try {
        const currW = sidebarBox.getBoundingClientRect().width;
        broadcastResize(currW, pendingHeight);
      } catch {
        /* noop */
      }
    };

    const onMouseMove = (e: MouseEvent) => {
      const deltaY = e.clientY - startY;
      const browserH =
        (document?.getElementById("browser") as HTMLElement | null)
          ?.clientHeight ?? window.innerHeight;
      const minH = 200;
      const maxH = browserH * 0.9;
      if (isTopSide) {
        const bottomEdge = startTop + startHeight;
        const desiredHeight = startHeight - deltaY;
        pendingHeight = Math.max(minH, Math.min(desiredHeight, maxH));
        pendingTop = Math.max(
          0,
          Math.min(browserH - pendingHeight, bottomEdge - pendingHeight),
        );
      } else {
        const desiredHeight = startHeight + deltaY;
        pendingHeight = Math.max(minH, Math.min(desiredHeight, maxH));
      }
      if (!frameRequested) {
        frameRequested = true;
        document?.defaultView?.requestAnimationFrame(applyFrame);
      }
    };

    const onMouseUp = () => {
      setIsFloatingDragging(false);
      document?.removeEventListener("mousemove", onMouseMove);
      document?.removeEventListener("mouseup", onMouseUp);
      docEl?.style.removeProperty("user-select");

      setIsResizeCooldown(true);

      if (resizeCooldownTimer !== null) {
        clearTimeout(resizeCooldownTimer);
      }

      resizeCooldownTimer = globalThis.setTimeout(() => {
        setIsResizeCooldown(false);
        resizeCooldownTimer = null;
      }, 1000);
    };

    docEl?.style.setProperty("user-select", "none");
    document?.addEventListener("mousemove", onMouseMove);
    document?.addEventListener("mouseup", onMouseUp);
  };

  const onDiagonalMouseDown = (e: MouseEvent) => {
    setIsFloatingDragging(true);
    const sidebarBox = document?.getElementById(
      "panel-sidebar-box",
    ) as XULElement;
    const docEl = document?.documentElement as XULElement | null;

    if (!sidebarBox) {
      return;
    }

    const startX = e.clientX;
    const startY = e.clientY;
    const startWidth = sidebarBox.getBoundingClientRect().width;
    const startHeight = sidebarBox.getBoundingClientRect().height;
    // 左上座標を offsetParent（= #browser）基準で算出
    const parentEl = (sidebarBox as unknown as HTMLElement)
      .offsetParent as HTMLElement | null;
    const parentRect = (
      parentEl ?? (document?.getElementById("browser") as HTMLElement)
    ).getBoundingClientRect();
    const boxRect = sidebarBox.getBoundingClientRect();
    const styleLeft = Number.parseInt(
      sidebarBox.style.getPropertyValue("left") || "NaN",
      10,
    );
    const styleTop = Number.parseInt(
      sidebarBox.style.getPropertyValue("top") || "NaN",
      10,
    );
    const startLeft = Number.isNaN(styleLeft)
      ? boxRect.left - parentRect.left
      : styleLeft;
    const startTop = Number.isNaN(styleTop)
      ? boxRect.top - parentRect.top
      : styleTop;
    const target = e.target as HTMLElement;

    const isTopLeft = target.classList.contains(
      "floating-splitter-corner-topleft",
    );
    const isTopRight = target.classList.contains(
      "floating-splitter-corner-topright",
    );
    const isBottomLeft = target.classList.contains(
      "floating-splitter-corner-bottomleft",
    );

    let frameRequested = false;
    let pendingWidth = startWidth;
    let pendingHeight = startHeight;
    let pendingLeft = startLeft;
    let pendingTop = startTop;

    const applyFrame = () => {
      frameRequested = false;
      sidebarBox.style.setProperty("width", `${pendingWidth}px`);
      sidebarBox.style.setProperty("height", `${pendingHeight}px`);
      if (isTopLeft || isBottomLeft) {
        sidebarBox.style.setProperty("left", `${pendingLeft}px`);
      }
      if (isTopLeft || isTopRight) {
        sidebarBox.style.setProperty("top", `${pendingTop}px`);
      }
      try {
        broadcastResize(pendingWidth, pendingHeight);
      } catch {
        /* noop */
      }
    };

    const onMouseMove = (e: MouseEvent) => {
      const deltaX = e.clientX - startX;
      const deltaY = e.clientY - startY;

      const browserW =
        (document?.getElementById("browser") as HTMLElement | null)
          ?.clientWidth ?? window.innerWidth;
      const browserH =
        (document?.getElementById("browser") as HTMLElement | null)
          ?.clientHeight ?? window.innerHeight;
      const minW = 225;
      const maxW = browserW * 0.8;
      const minH = 200;
      const maxH = browserH * 0.9;

      if (isTopLeft || isBottomLeft) {
        const rightEdge = startLeft + startWidth;
        const desiredWidth = startWidth - deltaX;
        pendingWidth = Math.max(minW, Math.min(desiredWidth, maxW));
        pendingLeft = Math.max(
          0,
          Math.min(browserW - pendingWidth, rightEdge - pendingWidth),
        );
      } else {
        const desiredWidth = startWidth + deltaX;
        pendingWidth = Math.max(minW, Math.min(desiredWidth, maxW));
      }

      if (isTopLeft || isTopRight) {
        const bottomEdge = startTop + startHeight;
        const desiredHeight = startHeight - deltaY;
        pendingHeight = Math.max(minH, Math.min(desiredHeight, maxH));
        pendingTop = Math.max(
          0,
          Math.min(browserH - pendingHeight, bottomEdge - pendingHeight),
        );
      } else {
        const desiredHeight = startHeight + deltaY;
        pendingHeight = Math.max(minH, Math.min(desiredHeight, maxH));
      }

      if (!frameRequested) {
        frameRequested = true;
        document?.defaultView?.requestAnimationFrame(applyFrame);
      }
    };

    const onMouseUp = () => {
      setIsFloatingDragging(false);
      document?.removeEventListener("mousemove", onMouseMove);
      document?.removeEventListener("mouseup", onMouseUp);
      docEl?.style.removeProperty("user-select");

      setIsResizeCooldown(true);

      if (resizeCooldownTimer !== null) {
        clearTimeout(resizeCooldownTimer);
      }

      resizeCooldownTimer = globalThis.setTimeout(() => {
        setIsResizeCooldown(false);
        resizeCooldownTimer = null;
      }, 1000);
    };

    docEl?.style.setProperty("user-select", "none");
    document?.addEventListener("mousemove", onMouseMove);
    document?.addEventListener("mouseup", onMouseUp);
  };

  createEffect(() => {
    const position = panelSidebarConfig().position_start;
    if (position) {
      document
        ?.getElementById("floating-splitter-right")
        ?.setAttribute("data-floating-splitter-side", "start");
      document
        ?.getElementById("floating-splitter-corner-bottomright")
        ?.setAttribute("data-floating-splitter-corner", "start");
    } else {
      document
        ?.getElementById("floating-splitter-right")
        ?.setAttribute("data-floating-splitter-side", "end");
      document
        ?.getElementById("floating-splitter-corner-bottomright")
        ?.setAttribute("data-floating-splitter-corner", "end");
    }
  });

  return (
    <>
      <xul:box
        id="floating-splitter-left"
        class="floating-splitter-side floating-splitter-left"
        part="floating-splitter-side"
        onMouseDown={onHorizontalMouseDown}
      />
      <xul:box
        id="floating-splitter-right"
        class="floating-splitter-side floating-splitter-right"
        part="floating-splitter-side"
        onMouseDown={onHorizontalMouseDown}
      />

      <xul:box
        id="floating-splitter-top"
        class="floating-splitter-vertical floating-splitter-top"
        part="floating-splitter-vertical"
        onMouseDown={onVerticalMouseDown}
      />
      <xul:box
        id="floating-splitter-bottom"
        class="floating-splitter-vertical floating-splitter-bottom"
        part="floating-splitter-vertical"
        onMouseDown={onVerticalMouseDown}
      />

      <xul:box
        id="floating-splitter-corner-topleft"
        class="floating-splitter-corner floating-splitter-corner-topleft"
        part="floating-splitter-corner"
        onMouseDown={onDiagonalMouseDown}
      />
      <xul:box
        id="floating-splitter-corner-topright"
        class="floating-splitter-corner floating-splitter-corner-topright"
        part="floating-splitter-corner"
        onMouseDown={onDiagonalMouseDown}
      />
      <xul:box
        id="floating-splitter-corner-bottomleft"
        class="floating-splitter-corner floating-splitter-corner-bottomleft"
        part="floating-splitter-corner"
        onMouseDown={onDiagonalMouseDown}
      />
      <xul:box
        id="floating-splitter-corner-bottomright"
        class="floating-splitter-corner floating-splitter-corner-bottomright"
        part="floating-splitter-corner"
        onMouseDown={onDiagonalMouseDown}
      />
    </>
  );
}
