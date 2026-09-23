// SPDX-License-Identifier: MPL-2.0

import { AppRegistryStore, parseAppRegistry } from "#libs/pwa/appRegistry.ts";
import type { AppRegistryStorage } from "#libs/pwa/appRegistryTypes.ts";

/**
 * Parent-process metadata only. Cookies, permissions and SSB manifests remain
 * in their existing stores. A Shim must never open this profile directly.
 */
export class AppRegistry {
  private static stores = new Map<string, AppRegistryStore>();

  static getForProfile(
    profileDirectory: string = PathUtils.profileDir,
  ): AppRegistryStore {
    if (
      Services.appinfo.processType !== Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
    ) {
      throw new Error(
        "[AppRegistry] Only the browser parent may open the registry",
      );
    }
    const directory = PathUtils.normalize(profileDirectory);
    let store = this.stores.get(directory);
    if (store) return store;
    const metadataDirectory = PathUtils.join(directory, "ssb");
    const filename = PathUtils.join(metadataDirectory, "app-registry.json");
    const storage: AppRegistryStorage = {
      async read() {
        if (!await IOUtils.exists(filename)) return null;
        // Parse here as well: a JSON `null` file is corruption, not a missing
        // registry, and must not allocate a new identity.
        return parseAppRegistry(await IOUtils.readJSON(filename));
      },
      async write(state) {
        await IOUtils.makeDirectory(metadataDirectory, {
          createAncestors: true,
          ignoreExisting: true,
          permissions: 0o700,
        });
        await IOUtils.writeJSON(filename, state, {
          tmpPath: `${filename}.tmp`,
          flush: true,
        });
      },
    };
    store = new AppRegistryStore(storage, directory);
    this.stores.set(directory, store);
    return store;
  }
}
