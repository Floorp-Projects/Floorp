// SPDX-License-Identifier: MPL-2.0

export const CONTEXT_MENU_SCHEMA_VERSION = 1 as const;

export const CONTEXT_MENU_ENABLED_PREF = "floorp.contextMenu.enabled";
export const CONTEXT_MENU_CONFIG_PREF = "floorp.contextMenu.config";

export type ContextMenuItemKey = string;
export type ContextMenuContainerKey = string;
export type ContextMenuProfileKey = string;
export type ContextMenuSurfaceKey = string;

/** A user override for one popup/container level. */
export interface ContextMenuLevelOverride {
  /**
   * Partial stable-key order. Only keys resolved as order anchors participate;
   * missing/new Firefox items remain in their current native slots.
   */
  order?: string[];
  /** Stable keys that receive the transient visibility overlay. */
  hidden?: string[];
}

/**
 * A context-specific override. Independent profiles use their own containers;
 * non-independent profiles use the surface base while keeping containers
 * dormant so switching modes does not destroy the user's profile settings.
 */
export interface ContextMenuProfileOverride {
  independent: boolean;
  containers: Record<ContextMenuContainerKey, ContextMenuLevelOverride>;
}

export interface ContextMenuSurfaceConfig {
  base: Record<ContextMenuContainerKey, ContextMenuLevelOverride>;
  profiles: Record<ContextMenuProfileKey, ContextMenuProfileOverride>;
}

export interface ContextMenuConfig {
  schemaVersion: typeof CONTEXT_MENU_SCHEMA_VERSION;
  surfaces: Record<ContextMenuSurfaceKey, ContextMenuSurfaceConfig>;
}

export type ContextMenuItemKind =
  | "command"
  | "submenu"
  | "separator"
  | "group";

export type ContextMenuItemSource =
  | "firefox"
  | "floorp"
  | "extension"
  | "unknown";

export interface ContextMenuItemDescriptor {
  key: ContextMenuItemKey;
  /**
   * Snapshot-local identity used only for rendering catalog rows. It is never
   * persisted. Unlike `key`, this remains unique when Firefox temporarily
   * exposes two nodes with the same stable identity.
   */
  catalogInstanceId?: string;
  label: string;
  kind: ContextMenuItemKind;
  source: ContextMenuItemSource;
  /**
   * Legacy combined capability. New consumers should use `movable` and
   * `hideable`; this remains false for separators so older settings pages do
   * not accidentally expose a separator visibility switch.
   */
  customizable: boolean;
  /** Whether the item itself may be moved by the user. */
  movable?: boolean;
  /** Whether the item may receive Floorp's transient hidden overlay. */
  hideable?: boolean;
  /**
   * Whether the stable key may participate in an order and accept drops.
   * Protected/read-only items remain false so their native slots are fixed.
   */
  orderAnchor?: boolean;
  nativeHidden: boolean;
  childContainerKey?: ContextMenuContainerKey;
}

export interface ContextMenuContainerDescriptor {
  key: ContextMenuContainerKey;
  label: string;
  complete: boolean;
  items: ContextMenuItemDescriptor[];
}

export interface ContextMenuProfileDescriptor {
  key: ContextMenuProfileKey;
  label: string;
  containers: ContextMenuContainerDescriptor[];
}

export interface ContextMenuSurfaceDescriptor {
  key: ContextMenuSurfaceKey;
  label: string;
  profiles: ContextMenuProfileDescriptor[];
}

export interface ContextMenuCatalogSnapshot {
  schemaVersion: typeof CONTEXT_MENU_SCHEMA_VERSION;
  revision: number;
  locale: string;
  surfaces: ContextMenuSurfaceDescriptor[];
}

/** Privileged process-wide catalog sink. Runtime loading is optional. */
export interface ContextMenuCatalogReporter {
  report(ownerId: string, snapshot: ContextMenuCatalogSnapshot): void;
  removeOwner(ownerId: string): void;
}

export interface EffectiveContextMenuLevelOverride {
  order: string[];
  hidden: string[];
}

export interface ContextMenuItemAlias {
  key: ContextMenuItemKey;
  selectors: readonly string[];
  source?: "firefox" | "floorp";
}

export interface ContextMenuAdapterProfile {
  key: ContextMenuProfileKey;
  label: string;
}

export interface ContextMenuAdapter {
  key: ContextMenuSurfaceKey;
  label: string;
  documentURIs: readonly string[];
  popupSelectors: readonly string[];
  aliases: readonly ContextMenuItemAlias[];
  readonlySelectors: readonly string[];
  profiles: readonly ContextMenuAdapterProfile[];
  getProfileKey(window: Window, rootPopup: Element): ContextMenuProfileKey;
  getContainerKey?(
    window: Window,
    popup: Element,
    rootPopup: Element,
  ): ContextMenuContainerKey | null;
}

export interface ContextMenuItemIdentity {
  key: ContextMenuItemKey;
  kind: ContextMenuItemKind;
  source: ContextMenuItemSource;
  customizable: boolean;
  movable: boolean;
  hideable: boolean;
  orderAnchor: boolean;
  childContainerKey?: ContextMenuContainerKey;
}
