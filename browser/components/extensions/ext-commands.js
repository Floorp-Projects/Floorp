/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
   PlatformInfo,
} = ExtensionUtils;

// WeakMap[Extension -> Map[name => Command]]
var commandsMap = new WeakMap();

function Command(description, shortcut) {
  this.description = description;
  this.shortcut = shortcut;
}

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_commands", (type, directive, extension, manifest) => {
  let commands = new Map();
  for (let name of Object.keys(manifest.commands)) {
    let os = PlatformInfo.os == "win" ? "windows" : PlatformInfo.os;
    let manifestCommand = manifest.commands[name];
    let description = manifestCommand.description;
    let shortcut = manifestCommand.suggested_key[os] || manifestCommand.suggested_key.default;
    let command = new Command(description, shortcut);
    commands.set(name, command);
  }
  commandsMap.set(extension, commands);
});

extensions.on("shutdown", (type, extension) => {
  commandsMap.delete(extension);
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("commands", null, (extension, context) => {
  return {
    commands: {
      getAll() {
        let commands = Array.from(commandsMap.get(extension), ([name, command]) => {
          return ({
            name,
            description: command.description,
            shortcut: command.shortcut,
          });
        });
        return Promise.resolve(commands);
      },
    },
  };
});
