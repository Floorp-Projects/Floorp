/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * NOTE: This file does not apply to builds in MC. This config is used for
 * our Jest tests and for webpack bundle builds.
 */
module.exports = {
  sourceType: "unambiguous",
  overrides: [
    {
      test: [
        "./src",
        "./packages/*/index.js",
        "./packages/*/src",
        /[/\\]node_modules[/\\]devtools-/,
        /[/\\]node_modules[/\\]react-aria-components[/\\]/,
        "../../shared",
      ],
      presets: [
        "@babel/preset-react",
        [
          "@babel/preset-env",
          {
            targets: {
              browsers: ["last 1 Chrome version", "last 1 Firefox version"],
            },
            modules: "commonjs",
          },
        ],
      ],
      plugins: [
        "@babel/plugin-proposal-class-properties",
        "@babel/plugin-proposal-optional-chaining",
        "@babel/plugin-proposal-nullish-coalescing-operator",
        "@babel/plugin-proposal-private-methods",
        "@babel/plugin-proposal-private-property-in-object",
        [
          "module-resolver",
          {
            alias: {
              "devtools/client/shared/vendor/react": "react",
              "devtools/client/shared/vendor/react-dom": "react-dom",
              "devtools/client/shared/vendor/react-dom-factories":
                "react-dom-factories",
              "devtools/client/shared/vendor/react-prop-types": "prop-types",
              // Map all require("devtools/...") to the real devtools root.
              "^devtools\\/(.*)": `${__dirname}/../../\\1`,
            },
          },
        ],
      ],
      env: {
        test: {
          presets: [
            [
              "@babel/preset-env",
              {
                targets: {
                  node: 7,
                },
                modules: "commonjs",
              },
            ],
          ],
        },
      },
    },
    {
      test: ["../shared/components"],
      plugins: [
        "@babel/plugin-proposal-class-properties",
        "@babel/plugin-proposal-optional-chaining",
        "@babel/plugin-proposal-nullish-coalescing-operator",
        "@babel/plugin-proposal-private-methods",
        "@babel/plugin-proposal-private-property-in-object",
        "transform-amd-to-commonjs",
      ],
    },
  ],
};
