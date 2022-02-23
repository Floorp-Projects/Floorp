/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const whatwgUrl = `
(() => {
  let factory;
  function define(...args) {
    if (factory) {
      throw new Error("expected a single define call");
    }

    if (
      args.length !== 2 ||
      !Array.isArray(args[0]) ||
      args[0].length !== 0 ||
      typeof args[1] !== "function"
    ) {
      throw new Error("whatwg-url had unexpected factory arguments.");
    }

    factory = args[1];
  }
  define.amd = true;

  const existingDefine = Object.getOwnPropertyDescriptor(globalThis, "define");
  globalThis.define = define;
  let err;
  try {
    importScripts("resource://devtools/client/shared/vendor/whatwg-url.js");

    if (!factory) {
      throw new Error("Failed to load whatwg-url factory");
    }
  } finally {
    if (existingDefine) {
      Object.defineProperty(globalThis, "define", existingDefine);
    } else {
      delete globalThis.define;
    }

  }

  return factory();
})()
`;

module.exports = {
  "./source-editor": "devtools/client/sourceeditor/editor",
  "../editor/source-editor": "devtools/client/sourceeditor/editor",
  immutable: "devtools/client/shared/vendor/immutable",
  lodash: "devtools/client/shared/vendor/lodash",
  react: "devtools/client/shared/vendor/react",
  "react-dom": "devtools/client/shared/vendor/react-dom",
  "react-dom-factories": "devtools/client/shared/vendor/react-dom-factories",
  "react-redux": "devtools/client/shared/vendor/react-redux",
  redux: "devtools/client/shared/vendor/redux",
  "prop-types": "devtools/client/shared/vendor/react-prop-types",
  "devtools-modules/src/menu": "devtools/client/framework/menu",
  "devtools-modules/src/menu/menu-item": "devtools/client/framework/menu-item",
  "devtools-services": "Services",
  "wasmparser/dist/cjs/WasmParser": "devtools/client/shared/vendor/WasmParser",
  "wasmparser/dist/cjs/WasmDis": "devtools/client/shared/vendor/WasmDis",
  "whatwg-url": `var ${whatwgUrl}`,
};
