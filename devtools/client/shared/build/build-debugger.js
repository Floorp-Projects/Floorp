/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const Babel = require("./babel");
const fs = require("fs");
const _path = require("path");

const mappings = {
  "./source-editor": "devtools/client/shared/sourceeditor/editor",
  "../editor/source-editor": "devtools/client/shared/sourceeditor/editor",
  react: "devtools/client/shared/vendor/react",
  "react-dom": "devtools/client/shared/vendor/react-dom",
  "react-dom-factories": "devtools/client/shared/vendor/react-dom-factories",
  "react-redux": "devtools/client/shared/vendor/react-redux",
  redux: "devtools/client/shared/vendor/redux",
  reselect: "devtools/client/shared/vendor/reselect",
  "prop-types": "devtools/client/shared/vendor/react-prop-types",
  "wasmparser/dist/cjs/WasmParser": "devtools/client/shared/vendor/WasmParser",
  "wasmparser/dist/cjs/WasmDis": "devtools/client/shared/vendor/WasmDis",
  "framework-actions": "devtools/client/framework/actions/index",
  "inspector-shared-utils": "devtools/client/inspector/shared/utils",
};

const mappingValues = Object.values(mappings);

// Add two additional mappings that cannot be reused when creating the
// webpack bundles.
mappings["devtools-source-map"] = "devtools/client/shared/source-map/index.js";

function isRequire(t, node) {
  return node && t.isCallExpression(node) && node.callee.name == "require";
}

// List of vendored modules.
// Should be synchronized with vendors.js
const VENDORS = [
  "classnames",
  "devtools-environment",
  "fuzzaldrin-plus",
  "react-aria-components/src/tabs",
  "react-transition-group/Transition",
  "Svg",
];

function shouldLazyLoad(value) {
  return (
    !value.includes("vendors") &&
    !value.includes("codemirror/") &&
    !value.endsWith(".properties") &&
    !value.startsWith("devtools/") &&
    // XXX: the lazyRequire rewriter (in transformMC) fails for this module, it
    // evaluates `t.thisExpression()` as `void 0` instead of `this`. But the
    // rewriter still works for other call sites and seems mandatory for the
    // debugger to start successfully (lazy requires help to break circular
    // dependencies).
    value !== "resource://gre/modules/AppConstants.jsm"
  );
}

/**
 * This Babel plugin is used to transpile a single Debugger module into a module that
 * can be loaded in Firefox via the regular DevTools loader.
 */
function transformMC({ types: t }) {
  return {
    visitor: {
      ModuleDeclaration(path, state) {
        const source = path.node.source;
        const value = source && source.value;
        if (value && value.includes(".css")) {
          path.remove();
        }
      },

      StringLiteral(path, state) {
        const { filePath } = state.opts;
        let value = path.node.value;

        if (!isRequire(t, path.parent)) {
          return;
        }

        // Handle require() to files mapped to other mozilla-central files.
        if (Object.keys(mappings).includes(value)) {
          path.replaceWith(t.stringLiteral(mappings[value]));
          return;
        }

        // Handle require() to files bundled in vendor.js.
        // e.g. require("some-module");
        //   -> require("devtools/client/debugger/dist/vendors").vendored["some-module"];
        const isVendored = VENDORS.some(vendored => value.endsWith(vendored));
        if (isVendored) {
          // components/shared/Svg is required using various relative paths.
          // Transform paths such as "../shared/Svg" to "Svg".
          if (value.endsWith("/Svg")) {
            value = "Svg";
          }

          // Transform the required path to require vendors.js
          path.replaceWith(
            t.stringLiteral("devtools/client/debugger/dist/vendors")
          );

          // Append `.vendored["some-module"]` after the require().
          path.parentPath.replaceWith(
            t.memberExpression(
              t.memberExpression(path.parent, t.identifier("vendored")),
              t.stringLiteral(value),
              true
            )
          );
          return;
        }

        // Handle implicit index.js requires:
        // in a node environment, require("my/folder") will automatically load
        // my/folder/index.js if available. The DevTools load does not handle
        // this case, so we need to explicitly transform such requires to point
        // to the index.js file.
        const dir = _path.dirname(filePath);
        const depPath = _path.join(dir, `${value}.js`);
        const exists = fs.existsSync(depPath);
        if (
          !exists &&
          !value.endsWith("index") &&
          !value.endsWith(".jsm") &&
          !(value.startsWith("devtools") || mappingValues.includes(value))
        ) {
          value = `${value}/index`;
          path.replaceWith(t.stringLiteral(value));
        }

        if (shouldLazyLoad(value)) {
          const requireCall = path.parentPath;
          const declarator = requireCall.parentPath;
          const declaration = declarator.parentPath;

          // require()s that are not assigned to a variable cannot be safely lazily required
          // since we lack anything to initiate the require (= the getter for the variable)
          if (declarator.type !== "VariableDeclarator") {
            return;
          }

          // update relative paths to be "absolute" (starting with devtools/)
          // e.g. ./utils/source-queue
          if (value.startsWith(".")) {
            // Create full path
            // e.g. z:\build\build\src\devtools\client\debugger\src\utils\source-queue
            let newValue = _path.join(_path.dirname(filePath), value);

            // Select the devtools portion of the path
            // e.g. devtools\client\debugger\src\utils\source-queue
            if (!newValue.startsWith("devtools")) {
              newValue = newValue.match(/^(.*?)(devtools.*)/)[2];
            }

            // Replace forward slashes with back slashes
            // e.g devtools/client/debugger/src/utils/source-queue
            newValue = newValue.replace(/\\/g, "/");

            value = newValue;
          }

          // rewrite to: loader.lazyRequireGetter(this, "variableName", "pathToFile")
          const lazyRequire = t.callExpression(
            t.memberExpression(
              t.identifier("loader"),
              t.identifier("lazyRequireGetter")
            ),
            [
              t.thisExpression(),
              t.stringLiteral(declarator.node.id.name || ""),
              t.stringLiteral(value),
            ]
          );

          declaration.replaceWith(lazyRequire);
        }
      },
    },
  };
}

Babel.registerPlugin("transform-mc", transformMC);

module.exports = function(filePath) {
  return [
    "proposal-optional-chaining",
    "proposal-class-properties",
    "transform-modules-commonjs",
    "transform-react-jsx",
    ["transform-mc", { mappings, vendors: VENDORS, filePath }],
  ];
};
