// SPDX-License-Identifier: MPL-2.0

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { createEffect, createSignal, onCleanup } from "solid-js";
import { enabled } from "../workspaces/data/config.ts";
import {
  selectedWorkspaceID,
  workspacesDataStore,
} from "../workspaces/data/data.ts";
import { WORKSPACES_CHANGED_OBSERVER_TOPIC } from "../workspaces/utils/workspaces-static-names.ts";
import { WorkspaceGestureController } from "./controller.ts";
import {
  commitWorkspaceSwitch,
  getWorkspaceSwitchSnapshot,
  registerWorkspaceGestureController,
} from "./runtime.ts";
import {
  getNativeWorkspaceGestureCapabilities,
  NATIVE_GESTURE_PREF,
  SIDEBAR_GESTURE_PREF,
  sidebarGestureTarget,
} from "./policy.ts";
import type {
  NativeWorkspaceSwipeEvent,
  SidebarWheelBurst,
  WorkspaceGesturePreview,
  WorkspaceSwitchTarget,
} from "./types.ts";
import i18next from "i18next";

export { showWorkspaceGestureSettings } from "./settings.ts";
export { getNativeWorkspaceGestureCapabilities } from "./policy.ts";
export { switchWorkspaceByOffset } from "./runtime.ts";

/** wheel has no physical finger count or reliable gesture phase. */
const SIDEBAR_QUIET_MS = 200;
const SIDEBAR_RELEASE_MS = 650;

@noraComponent(import.meta.hot)
export default class WorkspaceGestures extends NoraComponentBase {
  init(): void {
    const win = window;
    const doc = document;
    const [preview, setPreview] = createSignal<WorkspaceGesturePreview | null>(
      null,
    );
    const [announcement, setAnnouncement] = createSignal("");
    const overlay = doc.createElementNS("http://www.w3.org/1999/xhtml", "div");
    overlay.id = "floorp-workspace-switch-preview";
    overlay.setAttribute("aria-hidden", "true");
    overlay.style.cssText =
      "position:fixed;z-index:2147483646;bottom:28px;left:24px;max-width:380px;padding:12px 18px;border-radius:12px;background:var(--arrowpanel-background,#24242b);color:var(--arrowpanel-color,white);box-shadow:0 5px 28px #0005;border:1px solid var(--arrowpanel-border-color,#8885);pointer-events:none;font:menu;display:none;white-space:pre-line";
    const announcer = doc.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "div",
    );
    announcer.id = "floorp-workspace-switch-announcer";
    announcer.setAttribute("role", "status");
    announcer.setAttribute("aria-live", "polite");
    announcer.setAttribute("aria-atomic", "true");
    announcer.style.cssText =
      "position:fixed;width:1px;height:1px;overflow:hidden;clip-path:inset(50%);pointer-events:none";
    doc.documentElement.append(overlay, announcer);
    let hideTimer = 0;
    const renderCommit = (target: WorkspaceSwitchTarget): void => {
      const label = `${target.name} — ${target.sessionName}`;
      setAnnouncement(label);
      overlay.textContent = label;
      overlay.style.display = "block";
      overlay.style.transition =
        win.matchMedia("(prefers-reduced-motion: reduce)")?.matches
          ? "none"
          : "transform 140ms ease-out";
      overlay.style.transform = "translateX(0)";
      win.clearTimeout(hideTimer);
      hideTimer = win.setTimeout(() => {
        if (!preview()) overlay.style.display = "none";
      }, 400);
    };
    const controller = new WorkspaceGestureController({
      getSnapshot: () => getWorkspaceSwitchSnapshot(win),
      commit: (id) => commitWorkspaceSwitch(win, id),
      preview: setPreview,
      announce: renderCommit,
    });
    const unregister = registerWorkspaceGestureController(win, controller);
    const shortcutActions = {
      next: () => controller.switchByOffset(1),
      previous: () => controller.switchByOffset(-1),
    };
    Reflect.set(win, "floorpWorkspaceGestureActions", shortcutActions);
    createEffect(() => {
      const state = preview();
      if (!state) {
        overlay.style.display = "none";
        return;
      }
      win.clearTimeout(hideTimer);
      overlay.textContent = state.edge
        ? i18next.t("workspaceGestures.edge", {
          defaultValue: "End of workspaces",
        })
        : `${state.target.name} — ${state.target.sessionName}`;
      overlay.style.display = "block";
      overlay.style.transition = "none";
      overlay.style.transform =
        win.matchMedia("(prefers-reduced-motion: reduce)")?.matches
          ? "none"
          : `translateX(${Math.max(-30, Math.min(30, state.progress * 30))}px)`;
    });
    createEffect(() => {
      announcer.textContent = announcement();
    });
    createEffect(() => {
      enabled();
      selectedWorkspaceID();
      workspacesDataStore.order.join("|");
      workspacesDataStore.data;
      controller.validate();
    });

    let burst: SidebarWheelBurst | null = null;
    let finishTimer = 0;
    let releaseTimer = 0;
    let operation = 0;
    let locked = false;
    const usesNative = () =>
      Services.prefs.getBoolPref(NATIVE_GESTURE_PREF, false) &&
      getNativeWorkspaceGestureCapabilities(win).status === "available";
    const reset = (): void => {
      controller.cancel();
      burst = null;
      win.clearTimeout(finishTimer);
      win.clearTimeout(releaseTimer);
      locked = true;
      releaseTimer = win.setTimeout(() => {
        locked = false;
      }, SIDEBAR_RELEASE_MS);
    };
    const native = (rawEvent: Event): void => {
      const event = rawEvent as NativeWorkspaceSwipeEvent;
      if (
        !event.isTrusted || !usesNative() || !event.operationId ||
        !["start", "update", "end", "cancel"].includes(event.phase ?? "")
      ) return;
      if (event.fingerCount !== 3) {
        controller.cancel();
        return;
      }
      const consumed = controller.handle({
        source: "native",
        operationId: String(event.operationId),
        phase: event.phase as "start" | "update" | "end" | "cancel",
        displacementX: event.delta ?? 0,
        displacementY: event.deltaY ?? 0,
        time: win.performance.now(),
        fingerCount: event.fingerCount,
      });
      if (consumed) {
        event.preventDefault();
        event.stopImmediatePropagation();
      }
    };
    const wheel = (event: WheelEvent): void => {
      if (
        !event.isTrusted || !enabled() ||
        !Services.prefs.getBoolPref(SIDEBAR_GESTURE_PREF, false) ||
        usesNative()
      ) return;
      if (event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) {
        reset();
        return;
      }
      const now = win.performance.now();
      const target = sidebarGestureTarget(event.target, doc);
      if (!target) {
        // A wheel stream that began outside the sidebar may not become a
        // workspace gesture merely because the pointer crosses into it.
        reset();
        return;
      }
      if (locked) {
        win.clearTimeout(releaseTimer);
        releaseTimer = win.setTimeout(() => {
          locked = false;
        }, SIDEBAR_RELEASE_MS);
        return;
      }
      if (!burst) {
        burst = {
          operationId: String(++operation),
          x: 0,
          y: 0,
          startTarget: target,
          blocked: false,
          lastTime: now,
        };
        controller.handle({
          source: "sidebar",
          operationId: burst.operationId,
          phase: "start",
          displacementX: 0,
          displacementY: 0,
          time: now,
        });
      }
      if (burst.startTarget !== target) {
        reset();
        return;
      }
      const factor = event.deltaMode === WheelEvent.DOM_DELTA_LINE
        ? 16
        : event.deltaMode === WheelEvent.DOM_DELTA_PAGE
        ? 300
        : 1;
      burst.x -= event.deltaX * factor;
      burst.y += event.deltaY * factor;
      burst.lastTime = now;
      if (Math.abs(burst.y) >= 12 && Math.abs(burst.y) >= Math.abs(burst.x)) {
        burst.blocked = true;
      }
      const consumed = !burst.blocked &&
        controller.handle({
          source: "sidebar",
          operationId: burst.operationId,
          phase: "update",
          displacementX: burst.x,
          displacementY: burst.y,
          time: now,
        });
      if (consumed) {
        event.preventDefault();
        event.stopPropagation();
      }
      win.clearTimeout(finishTimer);
      finishTimer = win.setTimeout(() => {
        const ended = burst;
        burst = null;
        if (!ended) return;
        if (ended.blocked) controller.cancel();
        else {controller.handle({
            source: "sidebar",
            operationId: ended.operationId,
            phase: "end",
            displacementX: ended.x,
            displacementY: ended.y,
            time: ended.lastTime,
          });}
        locked = true;
        releaseTimer = win.setTimeout(() => {
          locked = false;
        }, SIDEBAR_RELEASE_MS);
      }, SIDEBAR_QUIET_MS);
    };
    const key = (event: KeyboardEvent): void => {
      if (event.key === "Escape") reset();
    };
    const observer = { observe: () => controller.validate() };
    const prefObserver = { observe: reset };
    Services.obs.addObserver(observer, WORKSPACES_CHANGED_OBSERVER_TOPIC);
    Services.prefs.addObserver("floorp.workspaces.gestures.", prefObserver);
    win.addEventListener("MozWorkspaceSwipeGesture", native, true);
    win.addEventListener("wheel", wheel, { capture: true, passive: false });
    win.addEventListener("blur", reset);
    win.addEventListener("dragstart", reset, true);
    win.addEventListener("keydown", key, true);
    win.addEventListener("MozMagnifyGestureStart", reset, true);
    win.addEventListener("MozRotateGestureStart", reset, true);
    onCleanup(() => {
      reset();
      unregister();
      win.clearTimeout(hideTimer);
      win.clearTimeout(releaseTimer);
      if (
        Reflect.get(win, "floorpWorkspaceGestureActions") === shortcutActions
      ) {
        Reflect.deleteProperty(win, "floorpWorkspaceGestureActions");
      }
      Services.obs.removeObserver(observer, WORKSPACES_CHANGED_OBSERVER_TOPIC);
      Services.prefs.removeObserver(
        "floorp.workspaces.gestures.",
        prefObserver,
      );
      win.removeEventListener("MozWorkspaceSwipeGesture", native, true);
      win.removeEventListener("wheel", wheel, true);
      win.removeEventListener("blur", reset);
      win.removeEventListener("dragstart", reset, true);
      win.removeEventListener("keydown", key, true);
      win.removeEventListener("MozMagnifyGestureStart", reset, true);
      win.removeEventListener("MozRotateGestureStart", reset, true);
      overlay.remove();
      announcer.remove();
    });
  }
}
