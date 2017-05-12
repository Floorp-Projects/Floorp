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
    // No eval() and no strings in the first param of setTimeout or setInterval
    "no-implied-eval": "error",
    "no-eval": "error",
  },
  // The html plugin is enabled via a command line option on eslint. To avoid
  // bad interactions with the xml preprocessor in eslint-plugin-mozilla, we
  // turn off processing of the html plugin for .xml files.
  "settings": {
    "html/xml-extensions": [ ".xhtml" ]
  },
  "env": {
    "es6": true
  },
  "parserOptions": {
    "ecmaVersion": 8,
  },
};
