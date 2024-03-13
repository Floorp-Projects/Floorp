/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node */

const path = require("path");
const webpack = require("webpack");
const rewriteChromeUri = require("./chrome-uri-utils.js");
const mdIndexer = require("./markdown-story-indexer.js");

const projectRoot = path.resolve(__dirname, "../../../../");

module.exports = {
  // The ordering for this stories array affects the order that they are displayed in Storybook
  stories: [
    // Docs section
    "../**/README.*.stories.md",
    // UI Widgets section
    `${projectRoot}/toolkit/content/widgets/**/*.stories.@(js|jsx|mjs|ts|tsx|md)`,
    // about:logins components stories
    `${projectRoot}/browser/components/aboutlogins/content/components/**/*.stories.mjs`,
    // Everything else
    "../stories/**/*.stories.@(js|jsx|mjs|ts|tsx|md)",
    // Design system files
    `${projectRoot}/toolkit/themes/shared/design-system/**/*.stories.@(js|jsx|mjs|ts|tsx|md)`,
  ],
  addons: [
    "@storybook/addon-links",
    {
      name: "@storybook/addon-essentials",
      options: {
        backgrounds: false,
        measure: false,
        outline: false,
      },
    },
    "@storybook/addon-a11y",
    path.resolve(__dirname, "addon-fluent"),
    path.resolve(__dirname, "addon-component-status"),
  ],
  framework: {
    name: "@storybook/web-components-webpack5",
    options: {},
  },

  experimental_indexers: async existingIndexers => {
    const customIndexer = {
      test: /(stories|story)\.md$/,
      createIndex: mdIndexer,
    };
    return [...existingIndexers, customIndexer];
  },
  webpackFinal: async (config, { configType }) => {
    // `configType` has a value of 'DEVELOPMENT' or 'PRODUCTION'
    // You can change the configuration based on that.
    // 'PRODUCTION' is used when building the static version of storybook.

    // Make whatever fine-grained changes you need
    config.resolve.alias = {
      browser: `${projectRoot}/browser`,
      toolkit: `${projectRoot}/toolkit`,
      "toolkit-widgets": `${projectRoot}/toolkit/content/widgets/`,
      "lit.all.mjs": `${projectRoot}/toolkit/content/widgets/vendor/lit.all.mjs`,
      react: "browser/components/storybook/node_modules/react",
      "react/jsx-runtime":
        "browser/components/storybook/node_modules/react/jsx-runtime",
      "@storybook/addon-docs":
        "browser/components/storybook/node_modules/@storybook/addon-docs",
    };

    config.plugins.push(
      // Rewrite chrome:// URI imports to file system paths.
      new webpack.NormalModuleReplacementPlugin(/^chrome:\/\//, resource => {
        resource.request = rewriteChromeUri(resource.request);
      })
    );

    config.module.rules.push({
      test: /\.ftl$/,
      type: "asset/source",
    });

    config.module.rules.push({
      test: /\.m?js$/,
      exclude: /.storybook/,
      use: [{ loader: path.resolve(__dirname, "./chrome-styles-loader.js") }],
    });

    // Replace the default CSS rule with a rule to emit a separate CSS file and
    // export the URL. This allows us to rewrite the source to use CSS imports
    // via the chrome-styles-loader.
    let cssFileTest = /\.css$/.toString();
    let cssRuleIndex = config.module.rules.findIndex(
      rule => rule.test.toString() === cssFileTest
    );
    config.module.rules[cssRuleIndex] = {
      test: /\.css$/,
      exclude: [/.storybook/, /node_modules/],
      type: "asset/resource",
      generator: {
        filename: "[name].[contenthash].css",
      },
    };

    // We're adding a rule for files matching this pattern in order to support
    // writing docs only stories in plain markdown.
    const MD_STORY_REGEX = /(stories|story)\.md$/;

    // Find the existing rule for MDX stories.
    let mdxStoryTest = /(stories|story)\.mdx$/.toString();
    let mdxRule = config.module.rules.find(
      rule => rule.test.toString() === mdxStoryTest
    );

    // Use a custom Webpack loader to transform our markdown stories into MDX,
    // then run our new MDX through the same loaders that Storybook usually uses
    // for MDX files. This is how we get a docs page from plain markdown.
    config.module.rules.push({
      test: MD_STORY_REGEX,
      use: [
        ...mdxRule.use,
        { loader: path.resolve(__dirname, "./markdown-story-loader.js") },
      ],
    });

    // Find the existing rule for markdown files.
    let markdownTest = /\.md$/.toString();
    let markdownRuleIndex = config.module.rules.findIndex(
      rule => rule.test.toString() === markdownTest
    );
    let markdownRule = config.module.rules[markdownRuleIndex];

    // Modify the existing markdown rule so it doesn't process .stories.md
    // files, but still treats any other markdown files as asset/source.
    config.module.rules[markdownRuleIndex] = {
      ...markdownRule,
      exclude: MD_STORY_REGEX,
    };

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
};
