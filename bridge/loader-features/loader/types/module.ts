// SPDX-License-Identifier: MPL-2.0

/**
 * Module Types
 *
 * Core type definitions for the loader module system.
 */

/** Shared type for modules loaded by the loader. */
export interface LoaderModule {
  name: string;
  init?: () => Promise<void>;
  initBeforeSessionStoreInit?: () => Promise<void>;
  default?: new () => unknown;
}

/** Module loader function type */
export type ModuleLoader = () => Promise<unknown>;

/** Module category record */
export type ModuleCategory = Record<string, ModuleLoader>;

/** All modules by category */
export interface ModulesRegistry {
  common: ModuleCategory;
  static: ModuleCategory;
}

/** Module keys by category */
export interface ModulesKeys {
  common: string[];
  static: string[];
}
