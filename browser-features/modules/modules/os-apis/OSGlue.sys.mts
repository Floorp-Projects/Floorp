/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * OSGlue - A service for initializing OS-level APIs
 *
 * This module initializes various OS-related services by importing them.
 */

class OSGlue {
  private _initialized = false;

  constructor() {
    if (this._initialized) {
      return;
    }

    this.initializeModules();
    this._initialized = true;
    console.log("[Floorp OS] Browser Side APIs initialized.");
  }

  private readonly modules = [
    this.localPathToResourceURI("./web/WebScraperServices.sys.mts"),
    this.localPathToResourceURI("./web/TabManagerServices.sys.mts"),
    this.localPathToResourceURI("./browser-info/BrowserInfo.sys.mts"),
  ];

  private localPathToResourceURI(path: string) {
    const re = /^\.\/([a-zA-Z0-9-_/]+)\.sys\.mts$/;
    const result = re.exec(path);
    if (!result || result.length !== 2) {
      throw new Error(
        `[nora-osGlue] localPathToResource URI match failed : ${path}`,
      );
    }
    const resourceURI = `resource://noraneko/modules/os-apis/${
      result[1]
    }.sys.mjs`;
    console.info(`[Floorp OS] Loading module: ${resourceURI}`);
    return resourceURI;
  }

  private initializeModules() {
    for (const module of this.modules) {
      ChromeUtils.importESModule(module);
    }
  }
}

/**
 * Singleton instance of OSGlue.
 * Importing this module will automatically initialize the services.
 */
export const osGlue = new OSGlue();
