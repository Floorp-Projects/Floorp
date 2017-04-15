/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

var {
  PlatformInfo,
} = ExtensionUtils;

var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

this.commands = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    let {extension} = this;

    this.id = makeWidgetId(extension.id);
    this.windowOpenListener = null;

    // Map[{String} commandName -> {Object} commandProperties]
    this.commands = this.loadCommandsFromManifest(this.extension.manifest);

    // WeakMap[Window -> <xul:keyset>]
    this.keysetsMap = new WeakMap();

    this.register();
    EventEmitter.decorate(this);
  }

  onShutdown(reason) {
    this.unregister();
  }

  /**
   * Registers the commands to all open windows and to any which
   * are later created.
   */
  register() {
    for (let window of windowTracker.browserWindows()) {
      this.registerKeysToDocument(window);
    }

    this.windowOpenListener = (window) => {
      if (!this.keysetsMap.has(window)) {
        this.registerKeysToDocument(window);
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
    let os = PlatformInfo.os == "win" ? "windows" : PlatformInfo.os;
    for (let [name, command] of Object.entries(manifest.commands)) {
      let suggested_key = command.suggested_key || {};
      let shortcut = suggested_key[os] || suggested_key.default;
      shortcut = shortcut ? shortcut.replace(/\s+/g, "") : null;
      commands.set(name, {
        description: command.description,
        shortcut,
      });
    }
    return commands;
  }

  /**
   * Registers the commands to a document.
   * @param {ChromeWindow} window The XUL window to insert the Keyset.
   */
  registerKeysToDocument(window) {
    let doc = window.document;
    let keyset = doc.createElementNS(XUL_NS, "keyset");
    keyset.id = `ext-keyset-id-${this.id}`;
    this.commands.forEach((command, name) => {
      if (command.shortcut) {
        let keyElement = this.buildKey(doc, name, command.shortcut);
        keyset.appendChild(keyElement);
      }
    });
    doc.documentElement.appendChild(keyset);
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
    let keyElement = this.buildKeyFromShortcut(doc, shortcut);

    // We need to have the attribute "oncommand" for the "command" listener to fire,
    // and it is currently ignored when set to the empty string.
    keyElement.setAttribute("oncommand", "//");

    /* eslint-disable mozilla/balanced-listeners */
    // We remove all references to the key elements when the extension is shutdown,
    // therefore the listeners for these elements will be garbage collected.
    keyElement.addEventListener("command", (event) => {
      let action;
      if (name == "_execute_page_action") {
        action = pageActionFor(this.extension);
      } else if (name == "_execute_browser_action") {
        action = browserActionFor(this.extension);
      } else if (name == "_execute_sidebar_action") {
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
   * @param {string} shortcut The shortcut provided in the manifest.
   *
   * @see https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/key
   * @returns {Document} The newly created Key element.
   */
  buildKeyFromShortcut(doc, shortcut) {
    let keyElement = doc.createElementNS(XUL_NS, "key");

    let parts = shortcut.split("+");

    // The key is always the last element.
    let chromeKey = parts.pop();

    // The modifiers are the remaining elements.
    keyElement.setAttribute("modifiers", this.getModifiersAttribute(parts));

    if (/^[A-Z0-9]$/.test(chromeKey)) {
      // We use the key attribute for all single digits and characters.
      keyElement.setAttribute("key", chromeKey);
    } else {
      keyElement.setAttribute("keycode", this.getKeycodeAttribute(chromeKey));
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
    let modifiersMap = {
      "Alt": "alt",
      "Command": "accel",
      "Ctrl": "accel",
      "MacCtrl": "control",
      "Shift": "shift",
    };
    return Array.from(chromeModifiers, modifier => {
      return modifiersMap[modifier];
    }).join(" ");
  }

  getAPI(context) {
    return {
      commands: {
        getAll: () => {
          let commands = this.commands;
          return Promise.resolve(Array.from(commands, ([name, command]) => {
            return ({
              name,
              description: command.description,
              shortcut: command.shortcut,
            });
          }));
        },
        onCommand: new SingletonEventManager(context, "commands.onCommand", fire => {
          let listener = (eventName, commandName) => {
            fire.async(commandName);
          };
          this.on("command", listener);
          return () => {
            this.off("command", listener);
          };
        }).api(),
      },
    };
  }
};
