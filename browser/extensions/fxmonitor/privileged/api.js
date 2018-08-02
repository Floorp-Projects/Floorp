ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

this.fxmonitor = class extends ExtensionAPI {
  getAPI(context) {
    const FirefoxMonitorContainer = {};
    Services.scriptloader.loadSubScript(
      context.extension.getURL("privileged/FirefoxMonitor.jsm"),
                               FirefoxMonitorContainer);
    return {
      fxmonitor: {
        async start() {
          FirefoxMonitorContainer.FirefoxMonitor.init(context.extension);
        },
      },
    };
  }
};
