/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "ExtensionParent",
                               "resource://gre/modules/ExtensionParent.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionSettingsStore",
                               "resource://gre/modules/ExtensionSettingsStore.jsm");

var {
  chromeModifierKeyMap,
  ExtensionError,
} = ExtensionUtils;

var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const EXECUTE_PAGE_ACTION = "_execute_page_action";
const EXECUTE_BROWSER_ACTION = "_execute_browser_action";
const EXECUTE_SIDEBAR_ACTION = "_execute_sidebar_action";

function normalizeShortcut(shortcut) {
  return shortcut ? shortcut.replace(/\s+/g, "") : null;
}

this.commands = class extends ExtensionAPI {
  static async onUninstall(extensionId) {
    // Cleanup the updated commands. In some cases the extension is installed
    // and uninstalled so quickly that `this.commands` hasn't loaded yet. To
    // handle that we need to make sure ExtensionSettingsStore is initialized
    // before we clean it up.
    await ExtensionSettingsStore.initialize();
    ExtensionSettingsStore
      .getAllForExtension(extensionId, "commands")
      .forEach(key => {
        ExtensionSettingsStore.removeSetting(extensionId, "commands", key);
      });
  }

  async onManifestEntry(entryName) {
    let {extension} = this;

    this.id = makeWidgetId(extension.id);
    this.windowOpenListener = null;

    // Map[{String} commandName -> {Object} commandProperties]
    this.manifestCommands = this.loadCommandsFromManifest(extension.manifest);

    this.commands = new Promise(async (resolve) => {
      // Deep copy the manifest commands to commands so we can keep the original
      // manifest commands and update commands as needed.
      let commands = new Map();
      this.manifestCommands.forEach((command, name) => {
        commands.set(name, {...command});
      });

      // Update the manifest commands with the persisted updates from
      // browser.commands.update().
      let savedCommands = await this.loadCommandsFromStorage(extension.id);
      savedCommands.forEach((update, name) => {
        let command = commands.get(name);
        if (command) {
          // We will only update commands, not add them.
          Object.assign(command, update);
        }
      });

      resolve(commands);
    });

    // WeakMap[Window -> <xul:keyset>]
    this.keysetsMap = new WeakMap();

    await this.register();
  }

  onShutdown(reason) {
    this.unregister();
  }

  registerKeys(commands) {
    for (let window of windowTracker.browserWindows()) {
      this.registerKeysToDocument(window, commands);
    }
  }

  /**
   * Registers the commands to all open windows and to any which
   * are later created.
   */
  async register() {
    let commands = await this.commands;
    this.registerKeys(commands);

    this.windowOpenListener = (window) => {
      if (!this.keysetsMap.has(window)) {
        this.registerKeysToDocument(window, commands);
      }
    };

    windowTracker.addOpenListener(this.windowOpenListener);
  }

  /**
   * Unregisters the commands from all open windows and stops commands
   * from being registered to windows which are later created.
   */
  unregister() {
    for (let window of windowTracker.browserWindows()) {
      if (this.keysetsMap.has(window)) {
        this.keysetsMap.get(window).remove();
      }
    }

    windowTracker.removeOpenListener(this.windowOpenListener);
  }

  /**
   * Creates a Map from commands for each command in the manifest.commands object.
   *
   * @param {Object} manifest The manifest JSON object.
   * @returns {Map<string, object>}
   */
  loadCommandsFromManifest(manifest) {
    let commands = new Map();
    // For Windows, chrome.runtime expects 'win' while chrome.commands
    // expects 'windows'.  We can special case this for now.
    let {PlatformInfo} = ExtensionParent;
    let os = PlatformInfo.os == "win" ? "windows" : PlatformInfo.os;
    for (let [name, command] of Object.entries(manifest.commands)) {
      let suggested_key = command.suggested_key || {};
      let shortcut = normalizeShortcut(suggested_key[os] || suggested_key.default);
      commands.set(name, {
        description: command.description,
        shortcut,
      });
    }
    return commands;
  }

  async loadCommandsFromStorage(extensionId) {
    await ExtensionSettingsStore.initialize();
    let names = ExtensionSettingsStore.getAllForExtension(extensionId, "commands");
    return names.reduce((map, name) => {
      let command = ExtensionSettingsStore.getSetting(
        "commands", name, extensionId).value;
      return map.set(name, command);
    }, new Map());
  }

  /**
   * Registers the commands to a document.
   * @param {ChromeWindow} window The XUL window to insert the Keyset.
   * @param {Map} commands The commands to be set.
   */
  registerKeysToDocument(window, commands) {
    let doc = window.document;
    let keyset = doc.createElementNS(XUL_NS, "keyset");
    keyset.id = `ext-keyset-id-${this.id}`;
    if (this.keysetsMap.has(window)) {
      this.keysetsMap.get(window).remove();
    }
    let sidebarKey;
    commands.forEach((command, name) => {
      if (command.shortcut) {
        let keyElement = this.buildKey(doc, name, command.shortcut);
        keyset.appendChild(keyElement);
        if (name == EXECUTE_SIDEBAR_ACTION) {
          sidebarKey = keyElement;
        }
      }
    });
    doc.documentElement.appendChild(keyset);
    if (sidebarKey) {
      window.SidebarUI.updateShortcut({key: sidebarKey});
    }
    this.keysetsMap.set(window, keyset);
  }

  /**
   * Builds a XUL Key element and attaches an onCommand listener which
   * emits a command event with the provided name when fired.
   *
   * @param {Document} doc The XUL document.
   * @param {string} name The name of the command.
   * @param {string} shortcut The shortcut provided in the manifest.
   * @see https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/key
   *
   * @returns {Document} The newly created Key element.
   */
  buildKey(doc, name, shortcut) {
    let keyElement = this.buildKeyFromShortcut(doc, name, shortcut);

    // We need to have the attribute "oncommand" for the "command" listener to fire,
    // and it is currently ignored when set to the empty string.
    keyElement.setAttribute("oncommand", "//");

    /* eslint-disable mozilla/balanced-listeners */
    // We remove all references to the key elements when the extension is shutdown,
    // therefore the listeners for these elements will be garbage collected.
    keyElement.addEventListener("command", (event) => {
      let action;
      if (name == EXECUTE_PAGE_ACTION) {
        action = pageActionFor(this.extension);
      } else if (name == EXECUTE_BROWSER_ACTION) {
        action = browserActionFor(this.extension);
      } else if (name == EXECUTE_SIDEBAR_ACTION) {
        action = sidebarActionFor(this.extension);
      } else {
        this.extension.tabManager
            .addActiveTabPermission();
        this.emit("command", name);
        return;
      }
      if (action) {
        let win = event.target.ownerGlobal;
        action.triggerAction(win);
      }
    });
    /* eslint-enable mozilla/balanced-listeners */

    return keyElement;
  }

  /**
   * Builds a XUL Key element from the provided shortcut.
   *
   * @param {Document} doc The XUL document.
   * @param {string} name The name of the shortcut.
   * @param {string} shortcut The shortcut provided in the manifest.
   *
   * @see https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/key
   * @returns {Document} The newly created Key element.
   */
  buildKeyFromShortcut(doc, name, shortcut) {
    let keyElement = doc.createElementNS(XUL_NS, "key");

    let parts = shortcut.split("+");

    // The key is always the last element.
    let chromeKey = parts.pop();

    // The modifiers are the remaining elements.
    keyElement.setAttribute("modifiers", this.getModifiersAttribute(parts));
    if (name == EXECUTE_SIDEBAR_ACTION) {
      let id = `ext-key-id-${this.id}-sidebar-action`;
      keyElement.setAttribute("id", id);
    }

    if (/^[A-Z]$/.test(chromeKey)) {
      // We use the key attribute for all single digits and characters.
      keyElement.setAttribute("key", chromeKey);
    } else {
      keyElement.setAttribute("keycode", this.getKeycodeAttribute(chromeKey));
      keyElement.setAttribute("event", "keydown");
    }

    return keyElement;
  }

  /**
   * Determines the corresponding XUL keycode from the given chrome key.
   *
   * For example:
   *
   *    input     |  output
   *    ---------------------------------------
   *    "PageUP"  |  "VK_PAGE_UP"
   *    "Delete"  |  "VK_DELETE"
   *
   * @param {string} chromeKey The chrome key (e.g. "PageUp", "Space", ...)
   * @returns {string} The constructed value for the Key's 'keycode' attribute.
   */
  getKeycodeAttribute(chromeKey) {
    if (/[0-9]/.test(chromeKey)) {
      return `VK_${chromeKey}`;
    }
    return `VK${chromeKey.replace(/([A-Z])/g, "_$&").toUpperCase()}`;
  }

  /**
   * Determines the corresponding XUL modifiers from the chrome modifiers.
   *
   * For example:
   *
   *    input             |   output
   *    ---------------------------------------
   *    ["Ctrl", "Shift"] |   "accel shift"
   *    ["MacCtrl"]       |   "control"
   *
   * @param {Array} chromeModifiers The array of chrome modifiers.
   * @returns {string} The constructed value for the Key's 'modifiers' attribute.
   */
  getModifiersAttribute(chromeModifiers) {
    return Array.from(chromeModifiers, modifier => {
      return chromeModifierKeyMap[modifier];
    }).join(" ");
  }

  getAPI(context) {
    return {
      commands: {
        getAll: async () => {
          let commands = await this.commands;
          return Array.from(commands, ([name, command]) => {
            return ({
              name,
              description: command.description,
              shortcut: command.shortcut,
            });
          });
        },
        update: async ({name, description, shortcut}) => {
          let {extension} = this;
          let commands = await this.commands;
          let command = commands.get(name);

          if (!command) {
            throw new ExtensionError(`Unknown command "${name}"`);
          }

          // Only store the updates so manifest changes can take precedence
          // later.
          let previousUpdates = await ExtensionSettingsStore.getSetting(
            "commands", name, extension.id);
          let commandUpdates = (previousUpdates && previousUpdates.value) || {};

          if (description && description != command.description) {
            commandUpdates.description = description;
            command.description = description;
          }

          if (shortcut && shortcut != command.shortcut) {
            shortcut = normalizeShortcut(shortcut);
            commandUpdates.shortcut = shortcut;
            command.shortcut = shortcut;
          }

          await ExtensionSettingsStore.addSetting(
            extension.id, "commands", name, commandUpdates);

          this.registerKeys(commands);
        },
        reset: async (name) => {
          let {extension, manifestCommands} = this;
          let commands = await this.commands;
          let command = commands.get(name);

          if (!command) {
            throw new ExtensionError(`Unknown command "${name}"`);
          }

          let storedCommand = ExtensionSettingsStore.getSetting(
            "commands", name, extension.id);

          if (storedCommand && storedCommand.value) {
            commands.set(name, {...manifestCommands.get(name)});
            ExtensionSettingsStore.removeSetting(extension.id, "commands", name);
            this.registerKeys(commands);
          }
        },
        onCommand: new EventManager({
          context,
          name: "commands.onCommand",
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
