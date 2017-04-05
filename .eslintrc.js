"use strict";

module.exports = {
  // When adding items to this file please check for effects on sub-directories.
  "plugins": [
    "mozilla"
  ],
  "rules": {
    "mozilla/avoid-removeChild": "error",
    "mozilla/avoid-nsISupportsString-preferences": "error",
    "mozilla/import-browser-window-globals": "error",
    "mozilla/import-globals": "warn",
    "mozilla/no-import-into-var-and-global": "error",
    "mozilla/no-useless-parameters": "error",
    "mozilla/no-useless-removeEventListener": "error",
    "mozilla/use-default-preference-values": "error",
    "mozilla/use-ownerGlobal": "error",

    // No (!foo in bar) or (!object instanceof Class)
    "no-unsafe-negation": "error",
  },
  "env": {
    "es6": true
  },
  "parserOptions": {
    "ecmaVersion": 8,
  },
};
