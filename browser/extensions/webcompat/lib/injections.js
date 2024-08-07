/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module */

class Injections {
  constructor(availableInjections, customFunctions) {
    this.INJECTION_PREF = "perform_injections";

    this._injectionsEnabled = true;

    this._availableInjections = availableInjections;
    this._activeInjections = new Set();
    // Only used if this.shouldUseScriptingAPI is false and we are falling back
    // to use the contentScripts API.
    this._activeInjectionHandles = new Map();
    this._customFunctions = customFunctions;

    this.shouldUseScriptingAPI =
      browser.aboutConfigPrefs.getBoolPrefSync("useScriptingAPI");
    // Debug log emit only on nightly (similarly to the debug
    // helper used in shims.js for similar purpose).
    browser.appConstants.getReleaseBranch().then(releaseBranch => {
      if (releaseBranch !== "release_or_beta") {
        console.debug(
          `WebCompat Injections will be injected using ${
            this.shouldUseScriptingAPI ? "scripting" : "contentScripts"
          } API`
        );
      }
    });
  }

  bindAboutCompatBroker(broker) {
    this._aboutCompatBroker = broker;
  }

  bootup() {
    browser.aboutConfigPrefs.onPrefChange.addListener(() => {
      this.checkInjectionPref();
    }, this.INJECTION_PREF);
    this.checkInjectionPref();
  }

  checkInjectionPref() {
    browser.aboutConfigPrefs.getPref(this.INJECTION_PREF).then(value => {
      if (value === undefined) {
        browser.aboutConfigPrefs.setPref(this.INJECTION_PREF, true);
      } else if (value === false) {
        this.unregisterContentScripts();
      } else {
        this.registerContentScripts();
      }
    });
  }

  getAvailableInjections() {
    return this._availableInjections;
  }

  isEnabled() {
    return this._injectionsEnabled;
  }

  async getPromiseRegisteredScriptIds(scriptIds) {
    let registeredScriptIds = [];

    // Try to avoid re-registering scripts already registered
    // (e.g. if the webcompat background page is restarted
    // after an extension process crash, after having registered
    // the content scripts already once), but do not prevent
    // to try registering them again if the getRegisteredContentScripts
    // method returns an unexpected rejection.
    try {
      const registeredScripts =
        await browser.scripting.getRegisteredContentScripts({
          // By default only look for script ids that belongs to Injections
          // (and ignore the ones that may belong to Shims).
          ids: scriptIds ?? this._availableInjections.map(inj => inj.id),
        });
      registeredScriptIds = registeredScripts.map(script => script.id);
    } catch (ex) {
      console.error(
        "Retrieve WebCompat GoFaster registered content scripts failed: ",
        ex
      );
    }

    return registeredScriptIds;
  }

  async registerContentScripts() {
    const platformInfo = await browser.runtime.getPlatformInfo();
    const platformMatches = [
      "all",
      platformInfo.os,
      platformInfo.os == "android" ? "android" : "desktop",
    ];

    let registeredScriptIds = this.shouldUseScriptingAPI
      ? await this.getPromiseRegisteredScriptIds()
      : [];

    for (const injection of this._availableInjections) {
      if (platformMatches.includes(injection.platform)) {
        injection.availableOnPlatform = true;
        await this.enableInjection(injection, registeredScriptIds);
      }
    }

    this._injectionsEnabled = true;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      interventionsChanged: this._aboutCompatBroker.filterOverrides(
        this._availableInjections
      ),
    });
  }

  buildContentScriptRegistrations(contentScripts) {
    let finalConfig = Object.assign({}, contentScripts);

    if (!finalConfig.runAt) {
      finalConfig.runAt = "document_start";
    }

    if (this.shouldUseScriptingAPI) {
      // Don't persist the content scripts across browser restarts
      // (at least not yet, we would need to apply some more changes
      // to adjust webcompat for accounting for the scripts to be
      // already registered).
      //
      // NOTE: scripting API has been introduced in Gecko 102,
      // prior to Gecko 105 persistAcrossSessions option was required
      // and only accepted false persistAcrossSessions, after Gecko 105
      // is optional and defaults to true.

      finalConfig.persistAcrossSessions = false;

      // Convert js/css from contentScripts.register API method
      // format to scripting.registerContentScripts API method
      // format.
      if (Array.isArray(finalConfig.js)) {
        finalConfig.js = finalConfig.js.map(e => e.file);
      }

      if (Array.isArray(finalConfig.css)) {
        finalConfig.css = finalConfig.css.map(e => e.file);
      }
    }

    return finalConfig;
  }

  async enableInjection(injection, registeredScriptIds) {
    if (injection.active) {
      return undefined;
    }

    if (injection.customFunc) {
      return this.enableCustomInjection(injection);
    }

    return this.enableContentScripts(injection, registeredScriptIds);
  }

  enableCustomInjection(injection) {
    if (injection.customFunc in this._customFunctions) {
      this._customFunctions[injection.customFunc](injection);
      injection.active = true;
    } else {
      console.error(
        `Provided function ${injection.customFunc} wasn't found in functions list`
      );
    }
  }

  async enableContentScripts(injection, registeredScriptIds) {
    let injectProps;
    try {
      const { id } = injection;
      if (this.shouldUseScriptingAPI) {
        // enableContentScripts receives a registeredScriptIds already
        // pre-computed once from registerContentScripts to register all
        // the injection, whereas it does not expect to receive one when
        // it is called from the AboutCompatBroker to re-enable one specific
        // injection.
        let activeScriptIds = Array.isArray(registeredScriptIds)
          ? registeredScriptIds
          : await this.getPromiseRegisteredScriptIds([id]);
        injectProps = this.buildContentScriptRegistrations(
          injection.contentScripts
        );
        injectProps.id = id;
        if (!activeScriptIds.includes(id)) {
          await browser.scripting.registerContentScripts([injectProps]);
        }
        this._activeInjections.add(id);
      } else {
        const handle = await browser.contentScripts.register(
          this.buildContentScriptRegistrations(injection.contentScripts)
        );
        this._activeInjections.add(id);
        this._activeInjectionHandles.set(id, handle);
      }

      injection.active = true;
    } catch (ex) {
      console.error(
        "Registering WebCompat GoFaster content scripts failed: ",
        { injection, injectProps },
        ex
      );
    }
  }

  unregisterContentScripts() {
    for (const injection of this._availableInjections) {
      this.disableInjection(injection);
    }

    this._injectionsEnabled = false;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      interventionsChanged: false,
    });
  }

  async disableInjection(injection) {
    if (!injection.active) {
      return undefined;
    }

    if (injection.customFunc) {
      return this.disableCustomInjections(injection);
    }

    return this.disableContentScripts(injection);
  }

  disableCustomInjections(injection) {
    const disableFunc = injection.customFunc + "Disable";

    if (disableFunc in this._customFunctions) {
      this._customFunctions[disableFunc](injection);
      injection.active = false;
    } else {
      console.error(
        `Provided function ${disableFunc} for disabling injection wasn't found in functions list`
      );
    }
  }

  async disableContentScripts(injection) {
    if (this._activeInjections.has(injection.id)) {
      if (this.shouldUseScriptingAPI) {
        await browser.scripting.unregisterContentScripts({
          ids: [injection.id],
        });
      } else {
        const handle = this._activeInjectionHandles.get(injection.id);
        await handle.unregister();
        this._activeInjectionHandles.delete(injection.id);
      }
      this._activeInjections.delete(injection);
    }
    injection.active = false;
  }
}

module.exports = Injections;
