// SPDX-License-Identifier: MPL-2.0

import type { ViteHotContext } from "vite/types/hot";
import { kebabCase } from "es-toolkit/string";
import type { ClassDecorator } from "./decorator";
import { createRootHMR, disposeRoot } from "@nora/solid-xul";
import { onCleanup } from "solid-js";

// U+2063 before `@` needed
//https://github.com/microsoft/TypeScript/issues/47679

/**
 * @exmaple ```ts
 * ⁣@noraComponent(import.meta.hot)
 * class FooBar extends NoraComponentBase {}
 * ```
 * @see {@link file://./../../vite.config.ts vite.config.ts} noraneko_component_hmr_support
 */
export function noraComponent(
  aViteHotContext: ViteHotContext | undefined,
): ClassDecorator<NoraComponentBase> {
  return (_clazz, ctx) => {
    const name = ctx.name;
    if (typeof name !== "string" || !name) {
      throw new Error("NoraComponent classes must have a name");
    }
    if (_NoraComponentBase_viteHotContext.has(name)) {
      throw new Error(`Duplicate NoraComponent Name: ${name}`);
    }

    _NoraComponentBase_viteHotContext.set(name, aViteHotContext);
    console.debug("[nora@base] noraComponent " + name);

    // Track which classes belong to this module's hot context.
    if (aViteHotContext) {
      let names = _classNamesByHotCtx.get(aViteHotContext);
      if (!names) {
        names = new Set();
        _classNamesByHotCtx.set(aViteHotContext, names);
      }
      names.add(name);

      // Register a single hot.dispose per module (Vite retains only the last
      // dispose callback per module — see vite #16283 — so we register once at
      // decoration time rather than per-instance in the constructor).
      // This guarantees that when the module is hot-updated, every live
      // instance's Solid root is torn down BEFORE the accept callback creates
      // a fresh instance. Without this, monkey-patched state (e.g.
      // TabDragDropManager's tabContainer listeners) leaks across HMR updates.
      if (!aViteHotContext.data.__noraDisposeRegistered) {
        aViteHotContext.data.__noraDisposeRegistered = true;
        aViteHotContext.data.__solidXulExternalDisposeOwner = true;
        aViteHotContext.dispose(() => {
          const hotCtx = aViteHotContext!;
          const classNames = _classNamesByHotCtx.get(hotCtx);
          if (classNames) {
            for (const className of classNames) {
              _noraInstancesByName.delete(className);
              _NoraComponentBase_viteHotContext.delete(className);
            }
            _classNamesByHotCtx.delete(hotCtx);
          }
          // Drain all Solid roots sharing this hot context. disposeRoot runs
          // every registered disposer, which fires each instance's onCleanup
          // (removing DOM nodes, event listeners, monkey-patches, etc.).
          disposeRoot(hotCtx);
          hotCtx.data.__noraDisposeRegistered = false;
          hotCtx.data.__solidXulDisposeRegistered = false;
          hotCtx.data.__solidXulExternalDisposeOwner = false;
          nora_component_base_console.debug(
            "hot.dispose: drained nora instances and solid roots for",
            classNames ? Array.from(classNames) : [],
          );
        });
      }
    }
  };
}

const nora_component_base_console = console.createInstance({
  prefix: `nora@nora-component-base`,
});

const _NoraComponentBase_viteHotContext = new Map<
  string,
  ViteHotContext | undefined
>();

/**
 * Registry of live NoraComponentBase instances, keyed by class name.
 * Populated in the constructor, drained on HMR dispose (via the decorator's
 * hot.dispose registration) and on individual instance cleanup.
 */
const _noraInstancesByName = new Map<string, Set<NoraComponentBase>>();

/**
 * Tracks which class names belong to which hot context, so the per-module
 * HMR dispose handler only drains instances for classes defined in the module
 * being hot-updated — not classes from unrelated modules.
 */
const _classNamesByHotCtx = new Map<ViteHotContext, Set<string>>();

export abstract class NoraComponentBase {
  logger: ConsoleInstance;
  constructor() {
    const name = this.constructor.name;

    // Register this instance so the HMR dispose handler can find it later.
    let instances = _noraInstancesByName.get(name);
    if (!instances) {
      instances = new Set();
      _noraInstancesByName.set(name, instances);
    }
    instances.add(this);

    // support HMR
    const hot = _NoraComponentBase_viteHotContext.get(name);
    // Initialize logger
    const _console = console.createInstance({
      prefix: `nora@${kebabCase(name)}`,
    });
    this.logger = _console;

    // Run init with solid-js HMR support
    createRootHMR(() => {
      this.init();
      onCleanup(() => {
        nora_component_base_console.debug(`onCleanup ${name}`);
        const instances = _noraInstancesByName.get(name);
        instances?.delete(this);
        if (instances?.size === 0) {
          _noraInstancesByName.delete(name);
        }
      });
    }, hot);
  }
  abstract init(): void;
}
