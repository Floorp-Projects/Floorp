import { ViteHotContext } from "vite/types/hot";
import { kebabCase } from 'es-toolkit/string';
import { } from "consola/browser";
import type { ClassDecorator } from "./decorator";
import { createRootHMR } from "@nora/solid-xul";
import { onCleanup } from "solid-js";

// U+2063 before `@` needed
//https://github.com/microsoft/TypeScript/issues/47679

/**
 * @exmaple ```ts
 * ⁣@noraComponent(import.meta.hot)
 * class FooBar extends NoraComponentBase {}
 * ```
 * @see {@link file://./../../vite.config.ts vite.config.ts} noraneko_component_hmr_support
 *
 */
export function noraComponent(aViteHotContext : ViteHotContext | undefined): ClassDecorator<NoraComponentBase> {
  return (clazz,ctx) => {
    if (_NoraComponentBase_viteHotContext.has(ctx.name!)) {
      throw new Error(`Duplicate NoraComponent Name: ${ctx.name}`);
    }

    _NoraComponentBase_viteHotContext.set(ctx.name!,aViteHotContext);
    console.debug("[nora@base] noraComponent "+ctx.name);
  }
}

const nora_component_base_console = console.createInstance({
  prefix:`nora@nora-component-base`,
});

let _NoraComponentBase_viteHotContext = new Map<string,ViteHotContext | undefined>()
export abstract class NoraComponentBase {
  logger: ConsoleInstance;
  constructor() {
    // support HMR
    const hot = _NoraComponentBase_viteHotContext.get(this.constructor.name);
    // Initialize logger
    const _console = console.createInstance({
      prefix:`nora@${kebabCase(this.constructor.name)}`,
    });
    this.logger = _console;

    // Run init with solid-js HMR support
    createRootHMR(()=>{
      this.init();
      onCleanup(()=>{
        nora_component_base_console.debug(`onCleanup ${this.constructor.name}`)
        _NoraComponentBase_viteHotContext.delete(this.constructor.name);
      });
    },hot);
  }
  abstract init(): void;
}