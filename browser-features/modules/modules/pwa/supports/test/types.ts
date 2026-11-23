// SPDX-License-Identifier: MPL-2.0

import type { Manifest } from "../../type.ts";
import type { WindowsSupport } from "../Windows.sys.mts";

export interface WindowsRaceStore {
  getCurrentSsbData(): Promise<Record<string, Manifest>>;
  saveSsbData(app: Manifest): Promise<void>;
  removeSsbData(key: string): Promise<void>;
}

export interface OwnedWindowsApp {
  app: Manifest;
  key: string;
  directory: string;
  shortcut: string;
}

export interface WindowsRaceFixture extends OwnedWindowsApp {
  Support: typeof WindowsSupport;
  store: WindowsRaceStore;
  pinCalls: string[];
  unpinCalls: string[];
  ownName(name: string): Promise<string>;
  ownApp(suffix: string): Promise<OwnedWindowsApp>;
  track<T>(operation: Promise<T>): Promise<T>;
  pauseIconLoads(): { entered: Promise<void>; release: () => void };
  removeLikeManager(support: WindowsSupport): Promise<void>;
  assertRemoved(): Promise<void>;
}

export interface WindowsCoordinator {
  withMutation<T>(id: string, operation: () => Promise<T>): Promise<T>;
  withStoreMutation<T>(operation: () => Promise<T>): Promise<T>;
}

export interface StoreAwareWindowsSupport {
  install(
    app: Manifest,
    store?: WindowsRaceStore,
    integrate?: boolean,
  ): Promise<void>;
  uninstall(
    app: Manifest,
    store?: WindowsRaceStore,
    integrate?: boolean,
  ): Promise<void>;
  rename(
    app: Manifest,
    newName: string,
    store?: WindowsRaceStore,
  ): Promise<boolean>;
}

export type StoreAwareSupport = {
  rename(
    app: Manifest,
    newName: string,
    store?: WindowsRaceStore,
  ): Promise<boolean>;
};

export type MockShell = {
  createShortcut: (...args: unknown[]) => Promise<unknown>;
  pinShortcutToTaskbar: (...args: unknown[]) => Promise<void>;
  unpinShortcutFromTaskbar: (...args: unknown[]) => void;
};
