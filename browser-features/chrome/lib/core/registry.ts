// SPDX-License-Identifier: MPL-2.0

/**
 * Module Registry - Data-Oriented Programming Style
 *
 * Central registry for runtime module management with hotswap support.
 * Separates pure logic from side-effectful operations.
 */

import type { ModuleHandle, ModuleInstance } from "./module_factory.ts";

// ============================================================================
// Types - Registry State
// ============================================================================

/**
 * Registry state (plain data)
 */
export interface RegistryState {
  /** Loaded module instances */
  modules: Map<string, ModuleInstance>;
  /** Module load order (for dependency resolution) */
  order: string[];
  /** Module URLs for reloading */
  urls: Map<string, string>;
}

/**
 * Dependency graph node
 */
interface DependencyNode {
  name: string;
  deps: string[];
  softDeps: string[];
}

// ============================================================================
// State - Global Registry
// ============================================================================

/** Global registry state */
const registry: RegistryState = {
  modules: new Map(),
  order: [],
  urls: new Map(),
};

/** Module dependency metadata (stored separately from instances) */
const moduleDeps: Map<string, { deps: string[]; softDeps: string[] }> = new Map();

// ============================================================================
// Pure Operations - Data Queries
// ============================================================================

/**
 * Get a module from the registry
 */
export const getModule = (name: string): ModuleInstance | undefined => {
  return registry.modules.get(name);
};

/**
 * Check if a module exists
 */
export const hasModule = (name: string): boolean => {
  return registry.modules.has(name);
};

/**
 * Get all module names
 */
export const getModuleNames = (): string[] => {
  return Array.from(registry.modules.keys());
};

/**
 * Get modules that depend on a given module (direct dependents only)
 */
export const getDependents = (name: string): string[] => {
  const dependents: string[] = [];

  for (const [moduleName, meta] of moduleDeps) {
    if (meta.deps.includes(name) || meta.softDeps.includes(name)) {
      dependents.push(moduleName);
    }
  }

  return dependents;
};

/**
 * Get all modules that transitively depend on a given module
 */
export const getTransitiveDependents = (name: string): string[] => {
  const result = new Set<string>();
  const queue = [name];

  while (queue.length > 0) {
    const current = queue.pop()!;
    for (const dependent of getDependents(current)) {
      if (!result.has(dependent)) {
        result.add(dependent);
        queue.push(dependent);
      }
    }
  }

  return Array.from(result);
};

/**
 * Get load order
 */
export const getLoadOrder = (): string[] => {
  return [...registry.order];
};

/**
 * Topologically sort modules by dependencies
 * Returns null if circular dependency detected
 */
export const topologicalSort = (nodes: DependencyNode[]): string[] | null => {
  const sorted: string[] = [];
  const visited = new Set<string>();
  const visiting = new Set<string>();
  const nodeMap = new Map(nodes.map((n) => [n.name, n]));

  const visit = (name: string): boolean => {
    if (visited.has(name)) return true;
    if (visiting.has(name)) return false; // Circular dependency

    visiting.add(name);

    const node = nodeMap.get(name);
    if (node) {
      // Visit hard dependencies
      for (const dep of node.deps) {
        if (!visit(dep)) return false;
      }
    }

    visiting.delete(name);
    visited.add(name);
    sorted.push(name);
    return true;
  };

  for (const node of nodes) {
    if (!visit(node.name)) {
      return null; // Circular dependency
    }
  }

  return sorted;
};

// ============================================================================
// Side-Effectful Operations - Module Management
// ============================================================================

/**
 * Load a module from URL
 * Side effect: fetches and evaluates module code
 */
export const loadModule = async (
  name: string,
  url: string,
): Promise<boolean> => {
  try {
    // Import module
    const module = await ChromeUtils.importESModule(url,{global:"current"});

    // Check if module exports default
    if (!module.default) {
      console.error(`[registry] Module "${name}" has no default export`);
      return false;
    }

    // Store URL for hotswap
    registry.urls.set(name, url);

    // If module is a handle, create and register instance
    const handle = module.default as ModuleHandle;
    if (handle.create) {
      const instance = handle.create();
      registry.modules.set(name, instance);

      // Store dependency metadata for reverse lookups
      if (handle.meta) {
        moduleDeps.set(name, {
          deps: handle.meta.deps ?? [],
          softDeps: handle.meta.softDeps ?? [],
        });
      }

      // Add to load order
      if (!registry.order.includes(name)) {
        registry.order.push(name);
      }

      // Initialize
      await instance.init();

      console.debug(`[registry] Loaded module: ${name}`);
      return true;
    }

    console.error(`[registry] Module "${name}" is not a valid handle`);
    return false;
  } catch (error) {
    console.error(`[registry] Failed to load module "${name}":`, error);
    return false;
  }
};

/**
 * Unload a module
 * Side effect: runs cleanup and removes from registry
 */
export const unloadModule = async (name: string): Promise<boolean> => {
  const instance = registry.modules.get(name);
  if (!instance) {
    return false;
  }

  try {
    // Run cleanup
    await instance.cleanup();

    // Remove from registry
    registry.modules.delete(name);
    registry.urls.delete(name);
    moduleDeps.delete(name);

    // Remove from load order
    const index = registry.order.indexOf(name);
    if (index !== -1) {
      registry.order.splice(index, 1);
    }

    console.debug(`[registry] Unloaded module: ${name}`);
    return true;
  } catch (error) {
    console.error(`[registry] Error unloading module "${name}":`, error);
    return false;
  }
};

/**
 * Hotswap a module (unload + reload)
 * Side effect: cleans up old instance, invalidates cache, loads new instance
 */
export const hotswapModule = async (
  name: string,
  newUrl?: string,
): Promise<boolean> => {
  const url = newUrl ?? registry.urls.get(name);
  if (!url) {
    console.error(`[registry] No URL found for module "${name}"`);
    return false;
  }

  console.debug(`[registry] Hotswapping module: ${name}`);

  // Unload old version
  await unloadModule(name);

  // Invalidate module cache by using a fresh URL (cache busting)
  // Cu.unload is not available in standard Firefox API
  try {
    const baseUrl = url.split("?")[0];
    const bustedUrl = `${baseUrl}?t=${Date.now()}`;
    return await loadModule(name, bustedUrl);
  } catch (e) {
    console.error(`[registry] Failed to hotswap module "${name}":`, e);
    return false;
  }
};

/**
 * Hotswap multiple modules
 * Side effect: hotswaps each module in sequence
 */
export const hotswapModules = async (
  names: string[],
): Promise<{ swapped: string[]; failed: string[] }> => {
  const swapped: string[] = [];
  const failed: string[] = [];

  for (const name of names) {
    const success = await hotswapModule(name);
    if (success) {
      swapped.push(name);
    } else {
      failed.push(name);
    }
  }

  return { swapped, failed };
};

/**
 * Cleanup all modules
 * Side effect: runs cleanup on all instances and clears registry
 */
export const cleanupAll = async (): Promise<void> => {
  console.debug("[registry] Cleaning up all modules...");

  // Cleanup in reverse load order
  const names = [...registry.order].reverse();

  for (const name of names) {
    await unloadModule(name);
  }

  // Clear registry
  registry.modules.clear();
  registry.order = [];
  registry.urls.clear();
  moduleDeps.clear();

  console.debug("[registry] All modules cleaned up");
};

/**
 * Get registry state (for debugging)
 */
export const getState = (): RegistryState => {
  return {
    modules: new Map(registry.modules),
    order: [...registry.order],
    urls: new Map(registry.urls),
  };
};
