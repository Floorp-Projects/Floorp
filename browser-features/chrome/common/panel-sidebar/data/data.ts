/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { effect, signal } from "@preact/signals";
import type { Signal } from "@preact/signals";
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
import { addDisposer, createRootHMR } from "#features-chrome/utils/base";
import { isRight } from "fp-ts/Either";

function defaultPanelSidebarData(): Panels {
  const result = zPanelSidebarData.decode(
    JSON.parse(strDefaultData) as unknown,
  );
  return isRight(result) ? result.right.data : [];
}

export function parsePanelSidebarData(stringData: string): Panels {
  try {
    const result = zPanelSidebarData.decode(
      JSON.parse(stringData) as unknown,
    );
    if (isRight(result)) {
      return result.right.data;
    }
    console.warn(
      "[PanelSidebar] Invalid panel data; restoring defaults.",
    );
  } catch (error) {
    console.warn(
      "[PanelSidebar] Failed to parse panel data; restoring defaults.",
      error,
    );
  }
  return defaultPanelSidebarData();
}

function getPanelSidebarConfigParsed(stringData: string): unknown {
  try {
    return JSON.parse(stringData);
  } catch (e) {
    console.error("Failed to parse panel sidebar config:", e);
    return {};
  }
}

function createPanelSidebarData(): Signal<Panels> {
  const sig = signal<Panels>(
    parsePanelSidebarData(
      Services.prefs.getStringPref(
        PanelSidebarStaticNames.panelSidebarDataPrefName,
        strDefaultData,
      ),
    ),
  );

  // sync signal → pref
  const disposeEffect = effect(() => {
    Services.prefs.setStringPref(
      PanelSidebarStaticNames.panelSidebarDataPrefName,
      JSON.stringify({ data: sig.value }),
    );
  });

  // sync pref → signal
  const observer = () => {
    const next = parsePanelSidebarData(
      Services.prefs.getStringPref(
        PanelSidebarStaticNames.panelSidebarDataPrefName,
        strDefaultData,
      ),
    );
    if (JSON.stringify(sig.peek()) !== JSON.stringify(next)) sig.value = next;
  };
  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarDataPrefName,
    observer,
  );

  addDisposer(() => {
    Services.prefs.removeObserver(
      PanelSidebarStaticNames.panelSidebarDataPrefName,
      observer,
    );
    disposeEffect();
  });

  return sig;
}

/** PanelSidebar data */
export const panelSidebarData: Signal<Panels> = createRootHMR(
  createPanelSidebarData,
  import.meta.hot,
);
export const setPanelSidebarData = (
  v: Panels | ((prev: Panels) => Panels),
): void => {
  panelSidebarData.value = typeof v === "function"
    ? v(panelSidebarData.value)
    : v;
};

function createSelectedPanelId(): Signal<string | null> {
  const sig = signal<string | null>(null);
  const disposeEffect = effect(() => {
    globalThis.gFloorpPanelSidebarCurrentPanel = sig.value;
  });
  addDisposer(() => {
    disposeEffect();
  });
  return sig;
}

/** Selected Panel */
export const selectedPanelId: Signal<string | null> = createRootHMR(
  createSelectedPanelId,
  import.meta.hot,
);
export const setSelectedPanelId = (v: string | null): void => {
  selectedPanelId.value = v;
};

function createPanelSidebarConfig(): Signal<PanelSidebarConfig> {
  const configResult = zPanelSidebarConfig.decode(
    getPanelSidebarConfigParsed(
      Services.prefs.getStringPref(
        PanelSidebarStaticNames.panelSidebarConfigPrefName,
        strDefaultConfig,
      ),
    ),
  );
  const sig = signal<PanelSidebarConfig>(
    isRight(configResult) ? configResult.right : JSON.parse(strDefaultConfig),
  );

  // sync signal → pref
  const disposeEffect = effect(() => {
    Services.prefs.setStringPref(
      PanelSidebarStaticNames.panelSidebarConfigPrefName,
      JSON.stringify(sig.value),
    );
  });

  // sync pref → signal
  const observer = () => {
    const result = zPanelSidebarConfig.decode(
      getPanelSidebarConfigParsed(
        Services.prefs.getStringPref(
          PanelSidebarStaticNames.panelSidebarConfigPrefName,
          strDefaultConfig,
        ),
      ),
    );
    if (
      isRight(result) &&
      JSON.stringify(sig.peek()) !== JSON.stringify(result.right)
    ) {
      sig.value = result.right;
    }
  };

  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarConfigPrefName,
    observer,
  );

  addDisposer(() => {
    Services.prefs.removeObserver(
      PanelSidebarStaticNames.panelSidebarConfigPrefName,
      observer,
    );
    disposeEffect();
  });

  return sig;
}

/** Get PanelSidebar Config data */
export const panelSidebarConfig: Signal<PanelSidebarConfig> = createRootHMR(
  createPanelSidebarConfig,
  import.meta.hot,
);
export const setPanelSidebarConfig = (
  v:
    | PanelSidebarConfig
    | ((previous: PanelSidebarConfig) => PanelSidebarConfig),
): void => {
  panelSidebarConfig.value = typeof v === "function"
    ? v(panelSidebarConfig.peek())
    : v;
};

/** Floating state */
export const isFloating: Signal<boolean> = createRootHMR(
  () => signal(false),
  import.meta.hot,
);
export const setIsFloating = (v: boolean): void => {
  isFloating.value = v;
};

/** Floating DraggingState */
export const isFloatingDragging: Signal<boolean> = createRootHMR(
  () => signal<boolean>(false),
  import.meta.hot,
);
export const setIsFloatingDragging = (v: boolean): void => {
  isFloatingDragging.value = v;
};

function createIsPanelSidebarEnabled(): Signal<boolean> {
  const sig = signal<boolean>(
    Services.prefs.getBoolPref(
      PanelSidebarStaticNames.panelSidebarEnabledPrefName,
      defaultEnabled,
    ),
  );

  // sync signal → pref
  const disposeEffect = effect(() => {
    Services.prefs.setBoolPref(
      PanelSidebarStaticNames.panelSidebarEnabledPrefName,
      sig.value,
    );
  });

  // sync pref → signal
  const observer = () => {
    sig.value = Services.prefs.getBoolPref(
      PanelSidebarStaticNames.panelSidebarEnabledPrefName,
      defaultEnabled,
    );
  };

  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarEnabledPrefName,
    observer,
  );

  addDisposer(() => {
    Services.prefs.removeObserver(
      PanelSidebarStaticNames.panelSidebarEnabledPrefName,
      observer,
    );
    disposeEffect();
  });

  return sig;
}

/** Panel Sidebar Enabled */
export const isPanelSidebarEnabled: Signal<boolean> = createRootHMR(
  createIsPanelSidebarEnabled,
  import.meta.hot,
);
export const setIsPanelSidebarEnabled = (v: boolean): void => {
  isPanelSidebarEnabled.value = v;
};
