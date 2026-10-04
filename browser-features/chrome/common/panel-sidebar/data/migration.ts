/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PanelSidebarStaticNames } from "../utils/panel-sidebar-static-names";
import type { PanelSidebarConfig } from "../utils/type";

type StringPrefSnapshot = {
  hasUserValue: boolean;
  value: string;
};

type StringPrefStore = {
  prefHasUserValue(name: string): boolean;
  getStringPref(name: string): string;
  setStringPref(name: string, value: string): void;
  clearUserPref(name: string): void;
};

type MigratedPreferenceValues = {
  legacyPrefName: string;
  dataPrefName: string;
  serializedSidebar: string;
  configPrefName: string;
  serializedConfig: string;
};

function snapshotStringPref(
  prefs: StringPrefStore,
  name: string,
): StringPrefSnapshot {
  const hasUserValue = prefs.prefHasUserValue(name);
  return {
    hasUserValue,
    value: hasUserValue ? prefs.getStringPref(name) : "",
  };
}

function restoreStringPref(
  prefs: StringPrefStore,
  name: string,
  snapshot: StringPrefSnapshot,
) {
  const hasUserValue = prefs.prefHasUserValue(name);
  if (
    hasUserValue === snapshot.hasUserValue &&
    (!hasUserValue || prefs.getStringPref(name) === snapshot.value)
  ) {
    return;
  }
  if (snapshot.hasUserValue) {
    prefs.setStringPref(name, snapshot.value);
  } else {
    prefs.clearUserPref(name);
  }
}

export function commitMigratedPreferences(
  prefs: StringPrefStore,
  values: MigratedPreferenceValues,
) {
  const snapshots = new Map<string, StringPrefSnapshot>([
    [values.dataPrefName, snapshotStringPref(prefs, values.dataPrefName)],
    [values.configPrefName, snapshotStringPref(prefs, values.configPrefName)],
    [values.legacyPrefName, snapshotStringPref(prefs, values.legacyPrefName)],
  ]);

  try {
    prefs.setStringPref(values.dataPrefName, values.serializedSidebar);
    prefs.setStringPref(values.configPrefName, values.serializedConfig);
    prefs.clearUserPref(values.legacyPrefName);
  } catch (error) {
    for (const [name, snapshot] of snapshots) {
      try {
        restoreStringPref(prefs, name, snapshot);
      } catch (rollbackError) {
        console.error(
          `[PanelSidebar] Failed to roll back preference ${name}.`,
          rollbackError,
        );
      }
    }
    throw error;
  }
}

export function migratePanelSidebarData() {
  const oldData = Services.prefs.getCharPref(
    "floorp.browser.sidebar2.data",
    undefined,
  );

  if (oldData) {
    try {
      const newSidebar = convertSidebar(JSON.parse(oldData) as OldSidebar);
      const serializedSidebar = JSON.stringify(newSidebar);

      // Create a new config
      const globalWidth = Services.prefs.getIntPref(
        "floorp.browser.sidebar2.global.webpanel.width",
        400,
      );

      const autoUnload = Services.prefs.getBoolPref(
        "floorp.browser.sidebar2.hide.to.unload.panel.enabled",
        false,
      );

      const position_start = Services.prefs.getBoolPref(
        "floorp.browser.sidebar.right",
        true,
      );

      let displayed = Services.prefs.getBoolPref(
        "floorp.browser.sidebar.is.displayed",
        true,
      );
      const enabled = Services.prefs.getBoolPref(
        "floorp.browser.sidebar.enable",
        true,
      );

      if (!enabled || !displayed) {
        // If either the old 'enable' pref or 'displayed' pref is false, set displayed to false.
        // This ensures that if the sidebar was previously disabled or hidden, it remains so.
        displayed = false;
      }

      const config: PanelSidebarConfig = {
        globalWidth,
        autoUnload,
        position_start,
        displayed,
        webExtensionRunningEnabled: false,
      };
      const serializedConfig = JSON.stringify(config);
      const legacyPrefName = "floorp.browser.sidebar2.data";
      commitMigratedPreferences(Services.prefs, {
        legacyPrefName,
        dataPrefName: PanelSidebarStaticNames.panelSidebarDataPrefName,
        serializedSidebar,
        configPrefName: PanelSidebarStaticNames.panelSidebarConfigPrefName,
        serializedConfig,
      });
    } catch (error) {
      console.warn(
        "[PanelSidebar] Failed to migrate legacy panel data; using the current panel configuration.",
        error,
      );
    }
  }
}

type OldSidebarData = {
  url: string;
  width?: number;
  usercontext?: number;
  zoomLevel?: number;
};

type OldSidebar = {
  data: { [key: string]: OldSidebarData };
  index: string[];
};

type NewSidebarItem = {
  id: string;
  type: "extension" | "static" | "web";
  width: number;
  url: string;
  userContextId: number | null;
  zoomLevel: number | null;
};

type NewSidebar = {
  data: NewSidebarItem[];
};

export function convertSidebar(oldSidebar: OldSidebar): NewSidebar {
  const newSidebar: NewSidebar = { data: [] };

  oldSidebar.index.forEach((key) => {
    const item = oldSidebar.data[key];
    const url = item.url;
    let type: "extension" | "static" | "web";
    const id = key;
    let width = item.width || 0;
    const userContextId = item.usercontext || null;
    const zoomLevel = item.zoomLevel || null;

    if (url.startsWith("extension")) {
      type = "extension";
      width = width || 450;
    } else if (url.startsWith("floorp//")) {
      type = "static";
    } else {
      type = "web";
    }

    newSidebar.data.push({
      id,
      type,
      width,
      url,
      userContextId,
      zoomLevel,
    });
  });

  return newSidebar;
}
