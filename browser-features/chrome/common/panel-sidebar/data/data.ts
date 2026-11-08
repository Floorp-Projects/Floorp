/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  type Accessor,
  createEffect,
  createSignal,
  onCleanup,
  type Setter,
} from "solid-js";
import {
  defaultEnabled,
  strDefaultConfig,
  strDefaultData,
} from "../utils/default-prerf.js";
import { PanelSidebarStaticNames } from "../utils/panel-sidebar-static-names.js";
import {
  type Panels,
  type PanelSidebarConfig,
  zPanelSidebarConfig,
  zPanelSidebarData,
} from "../utils/type.js";
import { createRootHMR } from "@nora/solid-xul";
import { pruneSavedUnreadBadges } from "../utils/saved-unread-badge.ts";
import i18next from "i18next";
import { isRight } from "fp-ts/Either";

function defaultPanelSidebarData(): Panels {
  const result = zPanelSidebarData.decode(
    JSON.parse(strDefaultData) as unknown,
  );
  return isRight(result) ? result.right.data : [];
}

/** Decode a panel preference without substituting fallback values. */
function decodePanelSidebarData(value: string): Panels | null {
  try {
    const result = zPanelSidebarData.decode(JSON.parse(value) as unknown);
    return isRight(result) ? result.right.data : null;
  } catch {
    return null;
  }
}

/** Parse stored panels for display, using defaults when the preference is invalid. */
export function parsePanelSidebarData(stringData: string): Panels {
  const panels = decodePanelSidebarData(stringData);
  if (panels) return panels;
  console.warn("[PanelSidebar] Invalid panel data; displaying defaults.");
  return defaultPanelSidebarData();
}

/** Synchronize panel preferences and remove saved counts for deleted panels. */
function createPanelSidebarData(): [Accessor<Panels>, Setter<Panels>] {
  const prefName = PanelSidebarStaticNames.panelSidebarDataPrefName;
  let invalidFallback: Panels | undefined;
  const read = () => {
    const raw = Services.prefs.getStringPref(prefName, strDefaultData);
    const panels = decodePanelSidebarData(raw);
    if (panels) {
      invalidFallback = undefined;
      return panels;
    }
    invalidFallback = defaultPanelSidebarData();
    return invalidFallback;
  };
  const [panelSidebarData, setPanelSidebarData] = createSignal<Panels>(read());
  const observer = () => setPanelSidebarData(read());
  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarDataPrefName,
    observer,
  );
  onCleanup(() => {
    Services.prefs.removeObserver(
      PanelSidebarStaticNames.panelSidebarDataPrefName,
      observer,
    );
  });

  createEffect(() => {
    const panels = panelSidebarData();
    const serialized = JSON.stringify({ data: panels });
    const restoreStoredPanels = () => {
      const stored = read();
      if (JSON.stringify({ data: stored }) !== serialized) {
        setPanelSidebarData(stored);
      }
    };
    if (Services.prefs.prefIsLocked(prefName)) {
      restoreStoredPanels();
      return;
    }
    const fallback = invalidFallback;
    if (fallback) {
      if (panels === fallback) return;
      const confirmed = Services.prompt.confirm(
        window as unknown as mozIDOMWindowProxy,
        i18next.t("panelSidebar.resetInvalidTitle"),
        i18next.t("panelSidebar.resetInvalidMessage"),
      );
      if (invalidFallback !== fallback) return;
      if (!confirmed) {
        setPanelSidebarData(fallback);
        return;
      }
    }
    try {
      Services.prefs.setStringPref(prefName, serialized);
    } catch (error) {
      console.error("[PanelSidebar] Failed to save panel settings", error);
      restoreStoredPanels();
      return;
    }
    if (
      !Services.prefs.prefIsLocked(prefName) &&
      Services.prefs.getStringPref(prefName, strDefaultData) === serialized
    ) {
      pruneSavedUnreadBadges(panels);
    } else {
      restoreStoredPanels();
    }
  });
  return [panelSidebarData, setPanelSidebarData];
}

/** PanelSidebar data */
export const [panelSidebarData, setPanelSidebarData] = createRootHMR(
  createPanelSidebarData,
  import.meta.hot,
);

function createSelectedPanelId(): [
  Accessor<string | null>,
  Setter<string | null>,
] {
  const [selectedPanelId, setSelectedPanelId] = createSignal<string | null>(
    null,
  );
  createEffect(() => {
    globalThis.gFloorpPanelSidebarCurrentPanel = selectedPanelId();
  });
  return [selectedPanelId, setSelectedPanelId];
}

/** Selected Panel */
export const [selectedPanelId, setSelectedPanelId] = createRootHMR(
  createSelectedPanelId,
  import.meta.hot,
);

function createPanelSidebarConfig(): [
  Accessor<PanelSidebarConfig>,
  Setter<PanelSidebarConfig>,
] {
  const configResult = zPanelSidebarConfig.decode(
    getPanelSidebarConfig(
      Services.prefs.getStringPref(
        PanelSidebarStaticNames.panelSidebarConfigPrefName,
        strDefaultConfig,
      ),
    ),
  );
  const [panelSidebarConfig, setPanelSidebarConfig] = createSignal<
    PanelSidebarConfig
  >(
    isRight(configResult) ? configResult.right : JSON.parse(strDefaultConfig),
  );
  createEffect(() => {
    Services.prefs.setStringPref(
      PanelSidebarStaticNames.panelSidebarConfigPrefName,
      JSON.stringify(panelSidebarConfig()),
    );
  });
  function getPanelSidebarConfig(stringData: string) {
    try {
      return JSON.parse(stringData);
    } catch (e) {
      console.error("Failed to parse panel sidebar config:", e);
      return {};
    }
  }
  const observer = () => {
    const result = zPanelSidebarConfig.decode(
      getPanelSidebarConfig(
        Services.prefs.getStringPref(
          PanelSidebarStaticNames.panelSidebarConfigPrefName,
          strDefaultConfig,
        ),
      ),
    );
    if (isRight(result)) {
      setPanelSidebarConfig(result.right);
    }
  };

  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarConfigPrefName,
    observer,
  );
  onCleanup(() => {
    Services.prefs.removeObserver(
      PanelSidebarStaticNames.panelSidebarConfigPrefName,
      observer,
    );
  });
  return [panelSidebarConfig, setPanelSidebarConfig];
}

/** Get PanelSidebar Config data */
export const [panelSidebarConfig, setPanelSidebarConfig] = createRootHMR(
  createPanelSidebarConfig,
  import.meta.hot,
);

/** Floating state */
export const [isFloating, setIsFloating] = createRootHMR(
  () => createSignal(false),
  import.meta.hot,
);

/** Floating DraggingState */
export const [isFloatingDragging, setIsFloatingDragging] = createRootHMR(
  () => createSignal<boolean>(false),
  import.meta.hot,
);

/** Hover previews keep their browser loaded while their presentation is closed. */
export const [isPanelSidebarHoverPreview, setIsPanelSidebarHoverPreview] =
  createRootHMR(() => createSignal(false), import.meta.hot);
export const [isPanelSidebarHoverOpen, setIsPanelSidebarHoverOpen] =
  createRootHMR(() => createSignal(false), import.meta.hot);
export const [isPanelSidebarResizing, setIsPanelSidebarResizing] =
  createRootHMR(() => createSignal(false), import.meta.hot);

export function isPanelSidebarOverlay(): boolean {
  return panelSidebarConfig().overlay === true && !isFloating();
}

export function isPanelSidebarVisible(): boolean {
  return selectedPanelId() !== null &&
    (!isPanelSidebarHoverPreview() || isPanelSidebarHoverOpen());
}

function createIsPanelSidebarEnabled(): [Accessor<boolean>, Setter<boolean>] {
  const [isPanelSidebarEnabled, setIsPanelSidebarEnabled] = createSignal<
    boolean
  >(
    Services.prefs.getBoolPref(
      PanelSidebarStaticNames.panelSidebarEnabledPrefName,
      defaultEnabled,
    ),
  );
  createEffect(() => {
    Services.prefs.setBoolPref(
      PanelSidebarStaticNames.panelSidebarEnabledPrefName,
      isPanelSidebarEnabled(),
    );
  });
  const observer = () =>
    setIsPanelSidebarEnabled(
      Services.prefs.getBoolPref(
        PanelSidebarStaticNames.panelSidebarEnabledPrefName,
        defaultEnabled,
      ),
    );
  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarEnabledPrefName,
    observer,
  );
  onCleanup(() => {
    Services.prefs.removeObserver(
      PanelSidebarStaticNames.panelSidebarEnabledPrefName,
      observer,
    );
  });

  return [isPanelSidebarEnabled, setIsPanelSidebarEnabled];
}

/** Panel Sidebar Enabled */
export const [isPanelSidebarEnabled, setIsPanelSidebarEnabled] = createRootHMR(
  createIsPanelSidebarEnabled,
  import.meta.hot,
);
