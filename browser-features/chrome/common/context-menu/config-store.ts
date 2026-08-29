// SPDX-License-Identifier: MPL-2.0

import {
  cloneContextMenuConfig,
  parseContextMenuConfig,
  serializeContextMenuConfig,
} from "./config.ts";
import {
  CONTEXT_MENU_CONFIG_PREF,
  CONTEXT_MENU_ENABLED_PREF,
  type ContextMenuConfig,
} from "./types.ts";

export interface ContextMenuPreferenceSource {
  getBoolPref(name: string, defaultValue?: boolean): boolean;
  getStringPref(name: string, defaultValue?: string): string;
  addObserver(name: string, observer: nsIObserver): void;
  removeObserver(name: string, observer: nsIObserver): void;
}

export interface ContextMenuConfigSnapshot {
  enabled: boolean;
  config: ContextMenuConfig;
}

export type ContextMenuConfigListener = (
  snapshot: ContextMenuConfigSnapshot,
) => void;

export class ContextMenuConfigStore {
  readonly #preferences: ContextMenuPreferenceSource;
  readonly #listeners = new Set<ContextMenuConfigListener>();
  #snapshot: ContextMenuConfigSnapshot;
  #observing = false;
  #destroyed = false;

  readonly #preferenceObserver: nsIObserver = (_subject, topic, data) => {
    if (topic !== "nsPref:changed") return;
    if (
      data !== CONTEXT_MENU_ENABLED_PREF && data !== CONTEXT_MENU_CONFIG_PREF
    ) {
      return;
    }
    this.reload();
  };

  constructor(preferences: ContextMenuPreferenceSource = Services.prefs) {
    this.#preferences = preferences;
    this.#snapshot = this.readSnapshot();
  }

  start(): void {
    if (this.#destroyed || this.#observing) return;
    try {
      this.#preferences.addObserver(
        CONTEXT_MENU_ENABLED_PREF,
        this.#preferenceObserver,
      );
      this.#preferences.addObserver(
        CONTEXT_MENU_CONFIG_PREF,
        this.#preferenceObserver,
      );
      this.#observing = true;
    } catch (error) {
      console.error(
        "[ContextMenuCustomizer] Failed to observe preferences",
        error,
      );
      this.stopObserving();
    }
  }

  getSnapshot(): ContextMenuConfigSnapshot {
    return {
      enabled: this.#snapshot.enabled,
      config: cloneContextMenuConfig(this.#snapshot.config),
    };
  }

  subscribe(listener: ContextMenuConfigListener): () => void {
    if (this.#destroyed) return () => {};
    this.#listeners.add(listener);
    return () => this.#listeners.delete(listener);
  }

  reload(): void {
    if (this.#destroyed) return;
    const next = this.readSnapshot();
    if (
      next.enabled === this.#snapshot.enabled &&
      serializeContextMenuConfig(next.config) ===
        serializeContextMenuConfig(this.#snapshot.config)
    ) {
      return;
    }
    this.#snapshot = next;
    for (const listener of this.#listeners) listener(this.getSnapshot());
  }

  destroy(): void {
    if (this.#destroyed) return;
    this.#destroyed = true;
    this.stopObserving();
    this.#listeners.clear();
  }

  private readSnapshot(): ContextMenuConfigSnapshot {
    let enabled = true;
    let serialized: string | null = null;
    try {
      enabled = this.#preferences.getBoolPref(
        CONTEXT_MENU_ENABLED_PREF,
        true,
      );
      serialized = this.#preferences.getStringPref(
        CONTEXT_MENU_CONFIG_PREF,
        "",
      );
    } catch (error) {
      console.error(
        "[ContextMenuCustomizer] Failed to read preferences",
        error,
      );
    }
    return {
      enabled,
      config: parseContextMenuConfig(serialized),
    };
  }

  private stopObserving(): void {
    try {
      this.#preferences.removeObserver(
        CONTEXT_MENU_ENABLED_PREF,
        this.#preferenceObserver,
      );
    } catch {
      // The first observer may not have been registered.
    }
    try {
      this.#preferences.removeObserver(
        CONTEXT_MENU_CONFIG_PREF,
        this.#preferenceObserver,
      );
    } catch {
      // The second observer may not have been registered.
    }
    this.#observing = false;
  }
}
