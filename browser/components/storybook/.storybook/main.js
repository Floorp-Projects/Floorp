/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node */

const path = require("path");
const projectRoot = path.resolve(__dirname, "../../../../");

// ./mach environment --format json
// topobjdir should be the build location

module.exports = {
  stories: [
    "../stories/**/*.stories.mdx",
    "../stories/**/*.stories.@(js|jsx|mjs|ts|tsx)",
    `${projectRoot}/toolkit/**/*.stories.@(js|jsx|mjs|ts|tsx)`,
  ],
  // Additions to the staticDirs might also need to get added to
  // MozXULElement.importCss in preview.mjs to enable auto-reloading.
  staticDirs: [
    `${projectRoot}/toolkit/content/widgets/`,
    `${projectRoot}/browser/themes/shared/`,
  ],
  addons: [
    "@storybook/addon-links",
    "@storybook/addon-essentials",
    "@storybook/addon-a11y",
  ],
  framework: "@storybook/web-components",
  webpackFinal: async (config, { configType }) => {
    // `configType` has a value of 'DEVELOPMENT' or 'PRODUCTION'
    // You can change the configuration based on that.
    // 'PRODUCTION' is used when building the static version of storybook.

    // Make whatever fine-grained changes you need
    config.resolve.alias.browser = `${projectRoot}/browser`;
    config.resolve.alias.toolkit = `${projectRoot}/toolkit`;
    config.resolve.alias[
      "toolkit-widgets"
    ] = `${projectRoot}/toolkit/content/widgets/`;

    config.module.rules.push({
      test: /\.ftl$/,
      type: "asset/source",
    });

    config.module.rules.push({
      test: /\.mjs/,
      loader: path.resolve(__dirname, "./chrome-uri-loader.js"),
    });

    config.optimization = {
      splitChunks: false,
      runtimeChunk: false,
      sideEffects: false,
      usedExports: false,
      concatenateModules: false,
      minimizer: [],
    };

    // Return the altered config
    return config;
  },
  core: {
    builder: "webpack5",
  },
};
