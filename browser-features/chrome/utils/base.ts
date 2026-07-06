// SPDX-License-Identifier: MPL-2.0

import type { ViteHotContext } from "vite/types/hot";
import { kebabCase } from "es-toolkit/string";
import type { ClassDecorator } from "./decorator.d.ts";
import { addDisposer, createRootHMR } from "@nora/preact-xul/lifetime";
export { addDisposer, createRootHMR } from "@nora/preact-xul/lifetime";

// U+2063 before `@` needed
//https://github.com/microsoft/TypeScript/issues/47679

/**
 * @exmaple ```ts
 * ⁣@noraComponent(import.meta.hot)
 * class FooBar extends NoraComponentBase {}
 * ```
 * @see {@link file://./../../vite.config.ts vite.config.ts} noraneko_component_hmr_support
 */
/**
 * 自己申告式：class 名を明示渡しで安定取得。
 * SWC の TC39 stage 3 decorator transform が _clazz.name / ctx.name を
 * 一部 class で空にする問題を回避する。
 *
 * @example ⁣@noraComponent("BrowserShareMode", import.meta.hot)
 */
export function noraComponent(
  name: string,
  aViteHotContext: ViteHotContext | undefined,
): ClassDecorator<NoraComponentBase> {
  return (_clazz, _ctx) => {
    if (_NoraComponentBase_viteHotContext.has(name)) {
      throw new Error(`Duplicate NoraComponent Name: ${name}`);
    }
    _NoraComponentBase_viteHotContext.set(name, aViteHotContext);
    console.debug("[nora@base] noraComponent " + name);
  };
}

const nora_component_base_console = console.createInstance({
  prefix: `nora@nora-component-base`,
});

const _NoraComponentBase_viteHotContext = new Map<
  string,
  ViteHotContext | undefined
>();
export abstract class NoraComponentBase {
  logger: ConsoleInstance;
  constructor() {
    // support HMR
    const hot = _NoraComponentBase_viteHotContext.get(this.constructor.name);
    // Initialize logger
    const _console = console.createInstance({
      prefix: `nora@${kebabCase(this.constructor.name)}`,
    });
    this.logger = _console;

    // Run init with preact HMR support
    createRootHMR(() => {
      this.init();
      addDisposer(() => {
        nora_component_base_console.debug(`dispose ${this.constructor.name}`);
        _NoraComponentBase_viteHotContext.delete(this.constructor.name);
      });
    }, hot);
  }
  abstract init(): void;
}
