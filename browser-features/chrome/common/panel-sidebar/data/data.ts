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
  zPanels,
  zPanelSidebarConfig,
} from "../utils/type.js";
import { createRootHMR } from "#features-chrome/utils/base";
import { isRight } from "fp-ts/Either";

function getPanelSidebarData(stringData: string) {
  return JSON.parse(stringData).data || {};
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
  const dataResult = zPanels.decode(
    getPanelSidebarData(
      Services.prefs.getStringPref(
        PanelSidebarStaticNames.panelSidebarDataPrefName,
        strDefaultData,
      ),
    ),
  );
  const sig = signal<Panels>(isRight(dataResult) ? dataResult.right : []);

  // sync signal → pref
  const disposeEffect = effect(() => {
    Services.prefs.setStringPref(
      PanelSidebarStaticNames.panelSidebarDataPrefName,
      JSON.stringify({ data: sig.value }),
    );
  });

  // sync pref → signal
  const observer = () => {
    const result = zPanels.decode(
      getPanelSidebarData(
        Services.prefs.getStringPref(
          PanelSidebarStaticNames.panelSidebarDataPrefName,
          strDefaultData,
        ),
      ),
    );
    if (isRight(result)) {
      sig.value = result.right;
    }
  };
  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarDataPrefName,
    observer,
  );

  import.meta.hot?.dispose(() => {
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
  panelSidebarData.value =
    typeof v === "function" ? v(panelSidebarData.value) : v;
};

function createSelectedPanelId(): Signal<string | null> {
  const sig = signal<string | null>(null);
  const disposeEffect = effect(() => {
    globalThis.gFloorpPanelSidebarCurrentPanel = sig.value;
  });
  import.meta.hot?.dispose(() => {
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
    if (isRight(result)) {
      sig.value = result.right;
    }
  };

  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarConfigPrefName,
    observer,
  );

  import.meta.hot?.dispose(() => {
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
export const setPanelSidebarConfig = (v: PanelSidebarConfig): void => {
  panelSidebarConfig.value = v;
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

  import.meta.hot?.dispose(() => {
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
