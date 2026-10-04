// SPDX-License-Identifier: MPL-2.0

import { createSignal, onCleanup } from "solid-js";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { panelSidebarConfig, setIsPanelSidebarResizing } from "../data/data.ts";

export function OverlaySplitter(props: { onResizeEnd: () => void }) {
  let cancelResize: (() => void) | undefined;
  const [label, setLabel] = createSignal(i18next.t("panelSidebar.resize"));
  addI18nObserver(() => setLabel(i18next.t("panelSidebar.resize")));
  const setWidth = (width: number): void => {
    const panel = document.getElementById("panel-sidebar-box");
    const browser = document.getElementById("browser");
    const rail = document.getElementById("panel-sidebar-select-box");
    if (!panel || !browser || !rail) return;
    const available = Math.max(
      0,
      browser.clientWidth - rail.getBoundingClientRect().width,
    );
    panel.style.width = `${
      Math.max(Math.min(225, available), Math.min(available, width))
    }px`;
  };
  const onMouseDown = (event: MouseEvent): void => {
    if (event.button !== 0) return;
    const panel = document.getElementById("panel-sidebar-box");
    const browser = document.getElementById("browser");
    const rail = document.getElementById("panel-sidebar-select-box");
    if (!panel || !browser || !rail) return;
    event.preventDefault();
    cancelResize?.();
    setIsPanelSidebarResizing(true);
    const startX = event.clientX;
    const startWidth = panel.getBoundingClientRect().width;
    const direction = panelSidebarConfig().position_start ? -1 : 1;
    const applyWidth = (move: MouseEvent): void => {
      setWidth(startWidth + direction * (move.clientX - startX));
    };
    const finish = (): void => {
      document.removeEventListener("mousemove", applyWidth, true);
      document.removeEventListener("mouseup", onMouseUp, true);
      globalThis.removeEventListener("blur", finish);
      cancelResize = undefined;
      setIsPanelSidebarResizing(false);
    };
    const onMouseUp = (up: MouseEvent): void => {
      applyWidth(up);
      props.onResizeEnd();
      finish();
    };
    cancelResize = finish;
    document.addEventListener("mousemove", applyWidth, true);
    document.addEventListener("mouseup", onMouseUp, true);
    globalThis.addEventListener("blur", finish);
  };
  onCleanup(() => cancelResize?.());
  return (
    <div
      id="panel-sidebar-overlay-splitter"
      role="separator"
      tabindex={0}
      aria-label={label()}
      aria-orientation="vertical"
      onMouseDown={onMouseDown}
      onKeyDown={(event: KeyboardEvent) => {
        if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
        event.preventDefault();
        const panel = document.getElementById("panel-sidebar-box");
        if (!panel) return;
        const direction = panelSidebarConfig().position_start ? -1 : 1;
        const delta = (event.key === "ArrowRight" ? 1 : -1) *
          (event.shiftKey ? 50 : 10);
        setWidth(panel.getBoundingClientRect().width + direction * delta);
        props.onResizeEnd();
      }}
    />
  );
}
