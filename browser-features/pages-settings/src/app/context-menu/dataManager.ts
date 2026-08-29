/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useCallback, useEffect, useRef, useState } from "react";
import { rpc } from "@/lib/rpc/rpc.ts";
import type {
  ContextMenuCatalogSnapshot,
  ContextMenuConfig,
  ContextMenuItemDescriptor,
} from "#features-chrome/common/context-menu/types.ts";
import {
  CONTEXT_MENU_CONFIG_PREF,
  CONTEXT_MENU_ENABLED_PREF,
} from "#features-chrome/common/context-menu/types.ts";
import { parseContextMenuConfigWithStatus } from "#features-chrome/common/context-menu/config.ts";
import {
  type ContextMenuPersistence,
  type ContextMenuPersistenceSnapshot,
  createContextMenuPersistence,
} from "./configPersistence.ts";
import {
  type ContextMenuLevelTarget,
  createDefaultContextMenuConfig,
  moveContextMenuItemBeforeKey,
  reorderContextMenuItemByKey,
  resetContextMenuProfile,
  setContextMenuItemHidden,
  setContextMenuProfileIndependent,
} from "./operations.ts";

export interface ContextMenuSettingsModel {
  enabled: boolean;
  config: ContextMenuConfig;
  catalog: ContextMenuCatalogSnapshot | null;
  loading: boolean;
  catalogLoading: boolean;
  pending: boolean;
  saveError: string | null;
  loadError: boolean;
  catalogError: boolean;
  toggleEnabled(): Promise<boolean>;
  setItemVisible(
    target: ContextMenuLevelTarget,
    itemKey: string,
    visible: boolean,
  ): Promise<boolean>;
  moveItem(
    target: ContextMenuLevelTarget,
    catalogItems: readonly ContextMenuItemDescriptor[],
    activeKey: string,
    overKey: string,
  ): Promise<boolean>;
  moveItemBefore(
    target: ContextMenuLevelTarget,
    catalogItems: readonly ContextMenuItemDescriptor[],
    activeKey: string,
    beforeKey?: string | null,
  ): Promise<boolean>;
  setProfileIndependent(
    surfaceKey: string,
    profileKey: string,
    independent: boolean,
  ): Promise<boolean>;
  resetProfile(surfaceKey: string, profileKey: string): Promise<boolean>;
  resetAll(): Promise<boolean>;
  reloadCatalog(): Promise<void>;
}

const DEFAULT_PERSISTENCE_SNAPSHOT: ContextMenuPersistenceSnapshot = {
  committed: {
    enabled: true,
    config: createDefaultContextMenuConfig(),
  },
  projected: {
    enabled: true,
    config: createDefaultContextMenuConfig(),
  },
  pending: false,
  error: null,
};

export function useContextMenuSettings(): ContextMenuSettingsModel {
  const [snapshot, setSnapshot] = useState<ContextMenuPersistenceSnapshot>(
    DEFAULT_PERSISTENCE_SNAPSHOT,
  );
  const [catalog, setCatalog] = useState<ContextMenuCatalogSnapshot | null>(
    null,
  );
  const [loading, setLoading] = useState(true);
  const [catalogLoading, setCatalogLoading] = useState(true);
  const [loadError, setLoadError] = useState(false);
  const [catalogError, setCatalogError] = useState(false);
  const persistenceRef = useRef<ContextMenuPersistence | null>(null);

  useEffect(() => {
    let cancelled = false;
    let unsubscribe: (() => void) | null = null;

    const loadPreferences = async () => {
      const [enabledResult, configResult] = await Promise.allSettled([
        rpc.getBoolPref(CONTEXT_MENU_ENABLED_PREF),
        rpc.getStringPref(CONTEXT_MENU_CONFIG_PREF),
      ]);
      if (cancelled) return;

      const enabled = enabledResult.status === "fulfilled"
        ? enabledResult.value ?? true
        : true;
      const parsedConfig = parseContextMenuConfigWithStatus(
        configResult.status === "fulfilled" ? configResult.value : null,
      );
      const config = parsedConfig.config;
      const unsafeConfig = parsedConfig.status === "invalid" ||
        parsedConfig.status === "unsupported-version";
      const preferencesFailed = enabledResult.status === "rejected" ||
        configResult.status === "rejected" || unsafeConfig;
      setLoadError(preferencesFailed);

      if (preferencesFailed) {
        // Never allow a fallback/default value to overwrite a preference that
        // merely failed to load. A later page reload can retry the read.
        setSnapshot({
          committed: { enabled, config },
          projected: { enabled, config },
          pending: false,
          error: null,
        });
      } else {
        const persistence = createContextMenuPersistence(
          { enabled, config },
          {
            writeEnabled: (nextEnabled) =>
              rpc.setBoolPref(CONTEXT_MENU_ENABLED_PREF, nextEnabled),
            writeConfig: (serializedConfig) =>
              rpc.setStringPref(CONTEXT_MENU_CONFIG_PREF, serializedConfig),
          },
        );
        persistenceRef.current = persistence;
        setSnapshot(persistence.getSnapshot());
        unsubscribe = persistence.subscribe(setSnapshot);
      }
      setLoading(false);
    };

    const loadCatalog = async () => {
      try {
        const initialCatalog = await rpc.getContextMenuCatalog();
        if (cancelled) return;
        setCatalog(initialCatalog);
        setCatalogError(false);
      } catch (error) {
        if (cancelled) return;
        console.error(
          "[ContextMenuSettings] Failed to load the catalog",
          error,
        );
        setCatalog(null);
        setCatalogError(true);
      } finally {
        if (!cancelled) setCatalogLoading(false);
      }
    };

    // Preference and catalog RPCs are independent: a menu that has not been
    // discovered yet must not delay the global enable/reset controls.
    void Promise.all([loadPreferences(), loadCatalog()]);
    return () => {
      cancelled = true;
      unsubscribe?.();
      persistenceRef.current = null;
    };
  }, []);

  const updateConfig = useCallback(
    (update: (current: Readonly<ContextMenuConfig>) => ContextMenuConfig) =>
      persistenceRef.current?.updateConfig(update) ?? Promise.resolve(false),
    [],
  );

  const toggleEnabled = useCallback(
    () =>
      persistenceRef.current?.updateEnabled((enabled) => !enabled) ??
        Promise.resolve(false),
    [],
  );

  const setItemVisible = useCallback(
    (
      target: ContextMenuLevelTarget,
      itemKey: string,
      visible: boolean,
    ) =>
      updateConfig((current) =>
        setContextMenuItemHidden(current, target, itemKey, !visible)
      ),
    [updateConfig],
  );

  const moveItem = useCallback(
    (
      target: ContextMenuLevelTarget,
      catalogItems: readonly ContextMenuItemDescriptor[],
      activeKey: string,
      overKey: string,
    ) =>
      updateConfig((current) =>
        reorderContextMenuItemByKey(
          current,
          target,
          catalogItems,
          activeKey,
          overKey,
        )
      ),
    [updateConfig],
  );

  const moveItemBefore = useCallback(
    (
      target: ContextMenuLevelTarget,
      catalogItems: readonly ContextMenuItemDescriptor[],
      activeKey: string,
      beforeKey?: string | null,
    ) =>
      updateConfig((current) =>
        moveContextMenuItemBeforeKey(
          current,
          target,
          catalogItems,
          activeKey,
          beforeKey,
        )
      ),
    [updateConfig],
  );

  const setProfileIndependent = useCallback(
    (surfaceKey: string, profileKey: string, independent: boolean) =>
      updateConfig((current) =>
        setContextMenuProfileIndependent(
          current,
          surfaceKey,
          profileKey,
          independent,
        )
      ),
    [updateConfig],
  );

  const resetProfile = useCallback(
    (surfaceKey: string, profileKey: string) =>
      updateConfig((current) =>
        resetContextMenuProfile(current, surfaceKey, profileKey)
      ),
    [updateConfig],
  );

  const resetAll = useCallback(
    () => updateConfig(() => createDefaultContextMenuConfig()),
    [updateConfig],
  );

  const reloadCatalog = useCallback(async () => {
    setCatalogLoading(true);
    try {
      const nextCatalog = await rpc.getContextMenuCatalog();
      setCatalog(nextCatalog);
      setCatalogError(false);
    } catch (error) {
      console.error(
        "[ContextMenuSettings] Failed to reload the catalog",
        error,
      );
      setCatalog(null);
      setCatalogError(true);
    } finally {
      setCatalogLoading(false);
    }
  }, []);

  return {
    enabled: snapshot.projected.enabled,
    config: snapshot.projected.config,
    catalog,
    loading,
    catalogLoading,
    pending: snapshot.pending,
    saveError: snapshot.error,
    loadError,
    catalogError,
    toggleEnabled,
    setItemVisible,
    moveItem,
    moveItemBefore,
    setProfileIndependent,
    resetProfile,
    resetAll,
    reloadCatalog,
  };
}
