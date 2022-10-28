/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DevToolsLoader } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs",
  {
    // `loadInDevToolsLoader` will import the loader in a special priviledged
    // global created for DevTools, which will be reused as the shared global
    // to load additional modules for the "DistinctSystemPrincipalLoader".
    loadInDevToolsLoader: true,
  }
);

// When debugging system principal resources (JSMs, chrome documents, ...)
// We have to load DevTools actors in another system principal global.
// That's mostly because of spidermonkey's Debugger API which requires
// debuggee and debugger to be in distinct principals.
//
// We try to hold a single instance of this special loader via this API.
//
// @param requester object
//        Object/instance which is using the loader.
//        The same requester object should be passed to release method.
let systemLoader = null;
const systemLoaderRequesters = new Set();

export function useDistinctSystemPrincipalLoader(requester) {
  if (!systemLoader) {
    systemLoader = new DevToolsLoader({
      useDevToolsLoaderGlobal: true,
    });
    systemLoaderRequesters.clear();
  }
  systemLoaderRequesters.add(requester);
  return systemLoader;
}

export function releaseDistinctSystemPrincipalLoader(requester) {
  systemLoaderRequesters.delete(requester);
  if (systemLoaderRequesters.size == 0) {
    systemLoader.destroy();
    systemLoader = null;
  }
}
