/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * test helper JSWindowActors used by the browser_startup_content_subframe.js test.
 */
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = [
  "StartupContentSubframeParent",
  "StartupContentSubframeChild",
];

class StartupContentSubframeParent extends JSWindowActorParent {
  receiveMessage(msg) {
    // Tell the test about the data we received from the content process.
    Services.obs.notifyObservers(
      msg.data,
      "startup-content-subframe-loaded-scripts"
    );
  }
}

class StartupContentSubframeChild extends JSWindowActorChild {
  async handleEvent(event) {
    // When the remote subframe is loaded, an event will be fired to this actor,
    // which will cause us to send the `LoadedScripts` message to the parent
    // process.
    // Wait a spin of the event loop before doing so to ensure we don't
    // miss any scripts loaded immediately after the load event.
    await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

    const Cm = Components.manager;
    Cm.QueryInterface(Ci.nsIServiceManager);
    const { AppConstants } = ChromeUtils.import(
      "resource://gre/modules/AppConstants.jsm"
    );
    let collectStacks = AppConstants.NIGHTLY_BUILD || AppConstants.DEBUG;

    let components = {};
    for (let component of Cu.loadedComponents) {
      // Keep only the file name for components, as the path is an absolute file
      // URL rather than a resource:// URL like for modules.
      components[component.replace(/.*\//, "")] = collectStacks
        ? Cu.getComponentLoadStack(component)
        : "";
    }

    let modules = {};
    for (let module of Cu.loadedModules) {
      modules[module] = collectStacks ? Cu.getModuleImportStack(module) : "";
    }

    let services = {};
    for (let contractID of Object.keys(Cc)) {
      try {
        if (Cm.isServiceInstantiatedByContractID(contractID, Ci.nsISupports)) {
          services[contractID] = "";
        }
      } catch (e) {}
    }
    this.sendAsyncMessage("LoadedScripts", {
      components,
      modules,
      services,
    });
  }
}
