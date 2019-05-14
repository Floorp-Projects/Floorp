/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "ExtensionShortcuts",
                               "resource://gre/modules/ExtensionShortcuts.jsm");

this.commands = class extends ExtensionAPI {
  static onUninstall(extensionId) {
    return ExtensionShortcuts.removeCommandsFromStorage(extensionId);
  }

  async onManifestEntry(entryName) {
    let shortcuts = new ExtensionShortcuts({
      extension: this.extension,
      onCommand: (name) => this.emit("command", name),
    });
    this.extension.shortcuts = shortcuts;
    await shortcuts.loadCommands();
    await shortcuts.register();
  }

  onShutdown() {
    this.extension.shortcuts.unregister();
  }

  getAPI(context) {
    return {
      commands: {
        getAll: () => this.extension.shortcuts.allCommands(),
        update: (args) => this.extension.shortcuts.updateCommand(args),
        reset: (name) => this.extension.shortcuts.resetCommand(name),
        onCommand: new EventManager({
          context,
          name: "commands.onCommand",
          inputHandling: true,
          register: fire => {
            let listener = (eventName, commandName) => {
              fire.async(commandName);
            };
            this.on("command", listener);
            return () => {
              this.off("command", listener);
            };
          },
        }).api(),
      },
    };
  }
};
