/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionShortcuts: "resource://gre/modules/ExtensionShortcuts.sys.mjs",
});

this.commands = class extends ExtensionAPIPersistent {
  PERSISTENT_EVENTS = {
    onCommand({ fire }) {
      let listener = (eventName, commandName) => {
        fire.async(commandName);
      };
      this.on("command", listener);
      return {
        unregister: () => this.off("command", listener),
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onChanged({ fire }) {
      let listener = (eventName, changeInfo) => {
        fire.async(changeInfo);
      };
      this.on("shortcutChanged", listener);
      return {
        unregister: () => this.off("shortcutChanged", listener),
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  static onUninstall(extensionId) {
    return ExtensionShortcuts.removeCommandsFromStorage(extensionId);
  }

  async onManifestEntry(entryName) {
    let shortcuts = new ExtensionShortcuts({
      extension: this.extension,
      onCommand: name => this.emit("command", name),
      onShortcutChanged: changeInfo => this.emit("shortcutChanged", changeInfo),
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
        update: args => this.extension.shortcuts.updateCommand(args),
        reset: name => this.extension.shortcuts.resetCommand(name),
        onCommand: new EventManager({
          context,
          module: "commands",
          event: "onCommand",
          inputHandling: true,
          extensionApi: this,
        }).api(),
        onChanged: new EventManager({
          context,
          module: "commands",
          event: "onChanged",
          extensionApi: this,
        }).api(),
      },
    };
  }
};
