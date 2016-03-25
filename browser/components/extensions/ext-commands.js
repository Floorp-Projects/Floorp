/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://devtools/shared/event-emitter.js");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  EventManager,
  PlatformInfo,
} = ExtensionUtils;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// WeakMap[Extension -> CommandList]
var commandsMap = new WeakMap();

function CommandList(commandsObj, extensionID) {
  this.commands = this.loadCommandsFromManifest(commandsObj);
  this.keysetID = `ext-keyset-id-${makeWidgetId(extensionID)}`;
  this.windowOpenListener = null;
  this.register();
  EventEmitter.decorate(this);
}

CommandList.prototype = {
  /**
   * Registers the commands to all open windows and to any which
   * are later created.
   */
  register() {
    for (let window of WindowListManager.browserWindows()) {
      this.registerKeysToDocument(window.document);
    }

    this.windowOpenListener = (window) => {
      this.registerKeysToDocument(window.document);
    };

    WindowListManager.addOpenListener(this.windowOpenListener);
  },

  /**
   * Unregisters the commands from all open windows and stops commands
   * from being registered to windows which are later created.
   */
  unregister() {
    for (let window of WindowListManager.browserWindows()) {
      let keyset = window.document.getElementById(this.keysetID);
      if (keyset) {
        keyset.remove();
      }
    }

    WindowListManager.removeOpenListener(this.windowOpenListener);
  },

  /**
   * Creates a Map from commands for each command in the manifest.commands object.
   * @param {Object} commandsObj The manifest.commands JSON object.
   */
  loadCommandsFromManifest(commandsObj) {
    let commands = new Map();
    // For Windows, chrome.runtime expects 'win' while chrome.commands
    // expects 'windows'.  We can special case this for now.
    let os = PlatformInfo.os == "win" ? "windows" : PlatformInfo.os;
    for (let name of Object.keys(commandsObj)) {
      let command = commandsObj[name];
      commands.set(name, {
        description: command.description,
        shortcut: command.suggested_key[os] || command.suggested_key.default,
      });
    }
    return commands;
  },

  /**
   * Registers the commands to a document.
   * @param {Document} doc The XUL document to insert the Keyset.
   */
  registerKeysToDocument(doc) {
    let keyset = doc.createElementNS(XUL_NS, "keyset");
    keyset.id = this.keysetID;
    this.commands.forEach((command, name) => {
      let keyElement = this.buildKey(doc, name, command.shortcut);
      keyset.appendChild(keyElement);
    });
    doc.documentElement.appendChild(keyset);
  },

  /**
   * Builds a XUL Key element and attaches an onCommand listener which
   * emits a command event with the provided name when fired.
   *
   * @param {Document} doc The XUL document.
   * @param {String} name The name of the command.
   * @param {String} shortcut The shortcut provided in the manifest.
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
      this.emit("command", name);
    });
    /* eslint-enable mozilla/balanced-listeners */

    return keyElement;
  },

  /**
   * Builds a XUL Key element from the provided shortcut.
   *
   * @param {Document} doc The XUL document.
   * @param {String} name The name of the command.
   * @param {String} shortcut The shortcut provided in the manifest.
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
  },

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
   * @param {String} key The chrome key (e.g. "PageUp", "Space", ...)
   * @return The constructed value for the Key's 'keycode' attribute.
   */
  getKeycodeAttribute(chromeKey) {
    return `VK${chromeKey.replace(/([A-Z])/g, "_$&").toUpperCase()}`;
  },

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
   * @return The constructed value for the Key's 'modifiers' attribute.
   */
  getModifiersAttribute(chromeModifiers) {
    let modifiersMap = {
      "Alt"     : "alt",
      "Command" : "accel",
      "Ctrl"    : "accel",
      "MacCtrl" : "control",
      "Shift"   : "shift",
    };
    return Array.from(chromeModifiers, modifier => {
      return modifiersMap[modifier];
    }).join(" ");
  },
};


/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_commands", (type, directive, extension, manifest) => {
  commandsMap.set(extension, new CommandList(manifest.commands, extension.id));
});

extensions.on("shutdown", (type, extension) => {
  let commandsList = commandsMap.get(extension);
  if (commandsList) {
    commandsList.unregister();
    commandsMap.delete(extension);
  }
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("commands", null, (extension, context) => {
  return {
    commands: {
      getAll() {
        let commands = commandsMap.get(extension).commands;
        return Promise.resolve(Array.from(commands, ([name, command]) => {
          return ({
            name,
            description: command.description,
            shortcut: command.shortcut,
          });
        }));
      },
      onCommand: new EventManager(context, "commands.onCommand", fire => {
        let listener = (event, name) => {
          fire(name);
        };
        commandsMap.get(extension).on("command", listener);
        return () => {
          commandsMap.get(extension).off("command", listener);
        };
      }).api(),
    },
  };
});
