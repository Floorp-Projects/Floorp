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
} from "./utils/default-prerf.ts";
import { PanelSidebarStaticNames } from "./utils/panel-sidebar-static-names.ts";
import {
  type Panels,
  type PanelSidebarConfig,
  zPanels,
  zPanelSidebarConfig,
  parseOrThrow,
} from "./utils/type.ts";
import { createRootHMR } from "@nora/solid-xul";

/**
 * Helpers to create pref-backed signals.
 * Keep implementation explicit and small so behavior is predictable.
 */

/* Generic JSON-pref signal that stores/reads a string pref and keeps it in sync.
   - read: calls readTransform on raw string from pref to produce value
   - write: writeTransform converts value to string to save
   - observer: listens to pref changes and updates the signal
*/
function createJsonPrefSignal<T>(
  prefName: string,
  readTransform: (raw: string) => T,
  writeTransform: (value: T) => string,
): [Accessor<T>, Setter<T>] {
  const readFromPref = () => {
    try {
      const raw = Services.prefs.getStringPref(prefName, writeTransform(null as unknown as T));
      return readTransform(raw);
    } catch (e) {
      console.error(`Failed to read pref ${prefName}:`, e);
      return readTransform(writeTransform(null as unknown as T));
    }
  };

  const [signal, setSignal] = createSignal<T>(readFromPref());

  const observer = () => {
    try {
      // Use updater form to avoid TS overload issues when T may be a function type.
      setSignal(() => readFromPref());
    } catch (e) {
      console.error(`Pref observer error for ${prefName}:`, e);
    }
  };

  Services.prefs.addObserver(prefName, observer);
  onCleanup(() => {
    Services.prefs.removeObserver(prefName, observer);
  });

  createEffect(() => {
    try {
      Services.prefs.setStringPref(prefName, writeTransform(signal()));
    } catch (e) {
      console.error(`Failed to write pref ${prefName}:`, e);
    }
  });

  return [signal, setSignal];
}

/* Boolean pref signal helper */
function createBoolPrefSignal(
  prefName: string,
  defaultValue: boolean,
): [Accessor<boolean>, Setter<boolean>] {
  const read = () =>
    Services.prefs.getBoolPref(prefName, defaultValue);

  const [signal, setSignal] = createSignal<boolean>(read());

  const observer = () => setSignal(read());
  Services.prefs.addObserver(prefName, observer);
  onCleanup(() => {
    Services.prefs.removeObserver(prefName, observer);
  });

  createEffect(() => {
    try {
      Services.prefs.setBoolPref(prefName, signal());
    } catch (e) {
      console.error(`Failed to write bool pref ${prefName}:`, e);
    }
  });

  return [signal, setSignal];
}

/* PanelSidebar data: stored as JSON with shape { data: Panels } in prefs.
   Use zPanels for validation via parseOrThrow.
*/
function createPanelSidebarData(): [Accessor<Panels>, Setter<Panels>] {
  const pref = PanelSidebarStaticNames.panelSidebarDataPrefName;

  const readTransform = (raw: string) => {
    // default raw is strDefaultData which already is a stringified object
    try {
      // maintain legacy shape: { data: Panels }
      const parsed = JSON.parse(raw) as { data?: unknown };
      const data = parsed?.data ?? [];
      return parseOrThrow(zPanels, data);
    } catch (e) {
      console.error("Failed to parse panel sidebar data from prefs:", e);
      // fallback to safe default
      return parseOrThrow(zPanels, JSON.parse(strDefaultData).data);
    }
  };

  const writeTransform = (value: Panels) => {
    try {
      return JSON.stringify({ data: value });
    } catch (e) {
      console.error("Failed to stringify panel sidebar data:", e);
      return strDefaultData;
    }
  };

  return createJsonPrefSignal<Panels>(pref, readTransform, writeTransform);
}

/* PanelSidebar config: stored as plain JSON object (not wrapped).
   Validate with zPanelSidebarConfig.
*/
function createPanelSidebarConfig(): [
  Accessor<PanelSidebarConfig>,
  Setter<PanelSidebarConfig>,
] {
  const pref = PanelSidebarStaticNames.panelSidebarConfigPrefName;

  const readTransform = (raw: string) => {
    try {
      const parsed = JSON.parse(raw);
      return parseOrThrow(zPanelSidebarConfig, parsed);
    } catch (e) {
      console.error("Failed to parse panel sidebar config:", e);
      try {
        return parseOrThrow(zPanelSidebarConfig, JSON.parse(strDefaultConfig));
      } catch {
        // fallback minimal config
        return {} as PanelSidebarConfig;
      }
    }
  };

  const writeTransform = (value: PanelSidebarConfig) => {
    try {
      return JSON.stringify(value);
    } catch (e) {
      console.error("Failed to stringify panel sidebar config:", e);
      return strDefaultConfig;
    }
  };

  return createJsonPrefSignal<PanelSidebarConfig>(
    pref,
    readTransform,
    writeTransform,
  );
}

/* Floating state and floating dragging state are simple signals saved in HMR roots */
export const [panelSidebarData, setPanelSidebarData] = createRootHMR(
  createPanelSidebarData,
  import.meta.hot,
);

/* Selected Panel id */
function createSelectedPanelId(): [
  Accessor<string | null>,
  Setter<string | null>,
] {
  const [selectedPanelId, setSelectedPanelId] = createSignal<string | null>(
    null,
  );
  createEffect(() => {
    // keep a global reference for other windows / debugging
    window.gFloorpPanelSidebarCurrentPanel = selectedPanelId();
  });
  return [selectedPanelId, setSelectedPanelId];
}

export const [selectedPanelId, setSelectedPanelId] = createRootHMR(
  createSelectedPanelId,
  import.meta.hot,
);

export const [panelSidebarConfig, setPanelSidebarConfig] = createRootHMR(
  createPanelSidebarConfig,
  import.meta.hot,
);

/* Floating state */
export const [isFloating, setIsFloating] = createRootHMR(
  () => createSignal(false),
  import.meta.hot,
);

/* Floating DraggingState */
export const [isFloatingDragging, setIsFloatingDragging] = createRootHMR(
  () => createSignal<boolean>(false),
  import.meta.hot,
);

/* Panel Sidebar Enabled: boolean pref */
function createIsPanelSidebarEnabled(): [Accessor<boolean>, Setter<boolean>] {
  return createBoolPrefSignal(
    PanelSidebarStaticNames.panelSidebarEnabledPrefName,
    defaultEnabled,
  );
}

export const [isPanelSidebarEnabled, setIsPanelSidebarEnabled] = createRootHMR(
  createIsPanelSidebarEnabled,
  import.meta.hot,
);