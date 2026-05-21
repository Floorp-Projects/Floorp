// SPDX-License-Identifier: MPL-2.0

/**
 * Module Definition - Simplified DOP/FP Style
 *
 * A lightweight module format with native hotswap support.
 * Julia/Kotlin-inspired patterns:
 * - Plain data structures for module definitions
 * - Pure functions for module creation
 * - Explicit state management
 * - Clear separation of concerns
 */

import { kebabCase } from "es-toolkit/string";
import type { ViteHotContext } from "vite/types/hot";

// ============================================================================
// Types - Module Definition
// ============================================================================

/**
 * Module definition with state and lifecycle
 */
export interface Module<S = unknown, E = unknown> {
  /** Module name (kebab-case preferred) */
  name: string;
  /** Module dependencies (loaded before this module) */
  deps?: string[];
  /** Soft dependencies (optional, don't block if missing) */
  softDeps?: string[];
  /** Factory for initial state (called on init) */
  state?: () => S;
  /** Event interface implementation */
  events?: E;
  /** Initialization function (side-effectful) */
  init?: (ctx: ModuleContext<S, E>) => void | Promise<void>;
  /** Cleanup function (side-effectful, required for hotswap) */
  cleanup?: (ctx: ModuleContext<S, E>) => void | Promise<void>;
}

/**
 * Context passed to module lifecycle hooks
 */
export interface ModuleContext<S = unknown, E = unknown> {
  /** Logger with module prefix */
  log: ConsoleInstance;
  /** Module name */
  name: string;
  /** Module state */
  state: S;
  /** Event interface */
  events: E;
}

/**
 * Module handle returned by module() factory
 */
export interface ModuleHandle<S = unknown, E = unknown> {
  /** Module metadata */
  readonly meta: {
    name: string;
    deps: string[];
    softDeps: string[];
  };
  /** Create module instance */
  create(): ModuleInstance<S, E>;
}

/**
 * Module instance (runtime representation)
 */
export interface ModuleInstance<S = unknown, E = unknown> {
  /** Module name */
  readonly name: string;
  /** Module context */
  readonly ctx: ModuleContext<S, E>;
  /** Initialize module */
  init(): void | Promise<void>;
  /** Cleanup module */
  cleanup(): void | Promise<void>;
}

// ============================================================================
// Pure Functions - Module Creation
// ============================================================================

/**
 * Create a module handle from a module definition
 *
 * @example
 * ```typescript
 * const sidebar = module({
 *   name: "sidebar",
 *   state: () => ({ icons: new Map() }),
 *   init(ctx) {
 *     ctx.log.debug("Sidebar ready");
 *   },
 *   cleanup(ctx) {
 *     ctx.state.icons.clear();
 *   },
 * });
 * ```
 */
export function module<S = unknown, E = unknown>(
  def: Module<S, E>,
): ModuleHandle<S, E> {
  return {
    meta: {
      name: def.name,
      deps: def.deps ?? [],
      softDeps: def.softDeps ?? [],
    },
    create(): ModuleInstance<S, E> {
      // Initialize state
      const state = def.state ? def.state() : (undefined as S);

      // Create context
      const ctx: ModuleContext<S, E> = {
        log: console.createInstance({
          prefix: `nora@${kebabCase(def.name)}`,
        }),
        name: def.name,
        state,
        events: def.events as E,
      };

      return {
        name: def.name,
        ctx,
        init: () => def.init?.(ctx),
        cleanup: () => def.cleanup?.(ctx),
      };
    },
  };
}

// ============================================================================
// Module Registry - Side-Effectful Operations
// ============================================================================

/** Active module instances */
const _instances: Map<string, ModuleInstance> = new Map();

/**
 * Register a module for runtime loading
 * Side effect: stores module handle in global registry
 */
export async function register<S, E>(handle: ModuleHandle<S, E>): Promise<void> {
  const name = handle.meta.name;

  // Check if already registered
  if (_instances.has(name)) {
    console.warn(`[module] Module "${name}" is already registered`);
    return;
  }

  // Create and initialize instance
  const instance = handle.create();
  _instances.set(name, instance);

  // Run init
  await instance.init();
}

/**
 * Unregister a module
 * Side effect: runs cleanup and removes from registry
 */
export async function unregister(name: string): Promise<boolean> {
  const instance = _instances.get(name);
  if (!instance) {
    return false;
  }

  // Run cleanup
  await instance.cleanup();

  // Remove from registry
  _instances.delete(name);
  return true;
}

/**
 * Get module instance (for testing/debugging)
 */
export function getInstance(name: string): ModuleInstance | undefined {
  return _instances.get(name);
}

/**
 * Check if module is registered
 */
export function hasModule(name: string): boolean {
  return _instances.has(name);
}

/**
 * Get all registered module names
 */
export function getModuleNames(): string[] {
  return Array.from(_instances.keys());
}

/**
 * Hotswap a module by unregistering and re-registering
 * Side effect: runs cleanup, removes, creates new instance, runs init
 */
export async function hotswap<S, E>(
  handle: ModuleHandle<S, E>,
): Promise<boolean> {
  const name = handle.meta.name;

  // Unregister old instance
  await unregister(name);

  // Register new instance
  await register(handle);

  return true;
}

/**
 * Cleanup all modules
 * Side effect: runs cleanup on all instances and clears registry
 */
export async function cleanupAll(): Promise<void> {
  console.debug("[module] Cleaning up all modules...");

  for (const [name, instance] of _instances) {
    try {
      await instance.cleanup();
    } catch (e) {
      console.error(`[module] Error cleaning up ${name}:`, e);
    }
  }

  _instances.clear();
}

// ============================================================================
// Unified API
// ============================================================================

/**
 * Register a module with unified definition and native HMR support
 *
 * @example
 * ```typescript
 * export default registerModule({
 *   name: "sidebar",
 *   init(ctx) { ... },
 *   cleanup(ctx) { ... }
 * }, import.meta);
 * ```
 */
export function registerModule<S = unknown, E = unknown>(
  def: Module<S, E>,
  meta?: ImportMeta | { hot?: any },
): ModuleHandle<S, E> {
  const handle = module(def);

  // Register immediately
  void register(handle);

  // Handle HMR (Vite)
  // Note: In production builds this is ignored as import.meta.hot is undefined
  const hot = meta && "hot" in meta ? meta.hot : undefined;

  if (hot) {
    hot.accept(async (newModule: any) => {
      if (newModule && newModule.default) {
        console.debug(`[module] HMR update for ${def.name}`);
        const newHandle = newModule.default;

        // Ensure it's a valid handle
        if (
          newHandle &&
          typeof newHandle === "object" &&
          "create" in newHandle
        ) {
          await hotswap(newHandle);
        }
      }
    });
  }

  return handle;
}
