/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function require_module(id) {
  if (!require_module.moduleLoader) {
    importScripts("/dom/quota/test/modules/worker/ModuleLoader.js");

    const base = location.href;

    const depth = "../../../../../";

    importScripts("/dom/quota/test/modules/worker/Assert.js");

    require_module.moduleLoader = new globalThis.ModuleLoader(base, depth);
  }

  return require_module.moduleLoader.require(id);
}
