/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const sourceMapAssets = require("devtools-source-map/assets");
const CopyWebpackPlugin = require("copy-webpack-plugin");
const webpack = require("webpack");

const getConfig = require("./bin/getConfig");
const mozillaCentralMappings = require("./configs/mozilla-central-mappings");
const path = require("path");
const ObjectRestSpreadPlugin = require("@sucrase/webpack-object-rest-spread-plugin");

const isProduction = process.env.NODE_ENV === "production";

/*
 * builds a path that's relative to the project path
 * returns an array so that we can prepend
 * hot-module-reloading in local development
 */
function getEntry(filename) {
  return [path.join(__dirname, filename)];
}

const webpackConfig = {
  entry: {
    // We always generate the debugger bundle, but we will only copy the CSS
    // artifact over to mozilla-central.
    "parser-worker": getEntry("src/workers/parser/worker.js"),
    "pretty-print-worker": getEntry("src/workers/pretty-print/worker.js"),
    "search-worker": getEntry("src/workers/search/worker.js"),
    "source-map-worker": getEntry("packages/devtools-source-map/src/worker.js"),
    "source-map-index": getEntry("packages/devtools-source-map/src/index.js"),
  },

  output: {
    path: path.join(__dirname, "assets/build"),
    filename: "[name].js",
    publicPath: "/assets/build",
  },

  plugins: [
    new CopyWebpackPlugin(
      Object.entries(sourceMapAssets).map(([name, filePath]) => ({
        from: filePath,
        to: `source-map-worker-assets/${name}`,
      }))
    ),
    new webpack.BannerPlugin({
      banner: `/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 `,
      raw: true,
      exclude: /\.css$/,
    }),
  ],
};

if (isProduction) {
  // In the firefox panel, build the vendored dependencies as a bundle instead,
  // the other debugger modules will be transpiled to a format that is
  // compatible with the DevTools Loader.
  webpackConfig.entry.vendors = getEntry("src/vendors.js");
}

const envConfig = getConfig();

const extra = {
  babelIncludes: ["react-aria-components"],
};

webpackConfig.plugins.push(new ObjectRestSpreadPlugin());

if (!isProduction) {
  webpackConfig.module = webpackConfig.module || {};
  webpackConfig.module.rules = webpackConfig.module.rules || [];
} else {
  webpackConfig.output.libraryTarget = "umd";
  extra.excludeMap = mozillaCentralMappings;
  extra.recordsPath = "bin/module-manifest.json";
}

const overallConfig = toolboxConfig(webpackConfig, envConfig, extra);

for (const rule of overallConfig.module.rules) {
  // The launchpad still uses Babel 6. Rewrite it to use the local Babel 7
  // install instead.
  if (rule.loader === "babel-loader?ignore=src/lib") {
    rule.loader = require.resolve("babel-loader");
    rule.options = {
      ignore: ["src/lib"],
    };
  }
}

module.exports = overallConfig;

function toolboxConfig(innerWebpackConfig, innerEnvConfig, options = {}) {
  require("babel-register");
  const ExtractTextPlugin = require("extract-text-webpack-plugin");
  const { isDevelopment, isFirefoxPanel } = require("devtools-environment");
  const { getValue, setConfig } = require("devtools-config");
  const NODE_ENV = process.env.NODE_ENV || "development";
  const TARGET = process.env.TARGET || "local";

  setConfig(innerEnvConfig);

  innerWebpackConfig.context = path.resolve(__dirname, "src");
  innerWebpackConfig.devtool = "source-map";

  innerWebpackConfig.module = innerWebpackConfig.module || {};
  innerWebpackConfig.module.rules = innerWebpackConfig.module.rules || [];
  innerWebpackConfig.module.rules.push({
    test: /\.json$/,
    loader: "json-loader",
  });

  innerWebpackConfig.module.rules.push({
    test: /\.js$/,
    exclude: request => {
      // Some paths are excluded from Babel
      const excludedPaths = ["fs", "node_modules"];
      const excludedRe = new RegExp(`(${excludedPaths.join("|")})`);
      let excluded = !!request.match(excludedRe);

      if (options.babelExcludes) {
        // If the tool defines an additional exclude regexp for Babel.
        excluded = excluded || !!request.match(options.babelExcludes);
      }

      let included = ["devtools-"];
      if (options.babelIncludes) {
        included = included.concat(options.babelIncludes);
      }

      const reincludeRe = new RegExp(
        `node_modules(\\/|\\\\)${included.join("|")}`
      );
      return excluded && !request.match(reincludeRe);
    },
    loader: `babel-loader?ignore=src/lib`,
  });

  innerWebpackConfig.module.rules.push({
    test: /\.properties$/,
    loader: "raw-loader",
  });

  innerWebpackConfig.node = { fs: "empty" };

  innerWebpackConfig.plugins = innerWebpackConfig.plugins || [];
  innerWebpackConfig.plugins.push(
    new webpack.DefinePlugin({
      "process.env": {
        NODE_ENV: JSON.stringify(NODE_ENV),
        TARGET: JSON.stringify(TARGET),
      },
      DebuggerConfig: JSON.stringify(innerEnvConfig),
    })
  );

  if (isDevelopment()) {
    /*
     * SVGs are loaded in one of two ways in JS w/ SVG inline loader
     * and in CSS w/ the CSS loader.
     *
     * Inline SVGs are included in the JS bundle and mounted w/ React.
     *
     * SVG URLs like chrome://devtools/skin/images/arrow.svg are mapped
     * by the postcss-loader to /mc/devtools/client/themes/arrow.svg
     * and are hosted in `development-server` with an express server.
     *
     * CSS URLs like resource://devtools/client/themes/variables.css are mapped by the
     * NormalModuleReplacementPlugin to
     * devtools-mc-assets/assets/devtools/client/themes/arrow.svg.
     * The modules are then resolved by the css-loader, which allows modules like
     * variables.css to be bundled.
     *
     * We use several PostCSS plugins to make local development a little easier:
     * autoprefixer, bidirection. These plugins help support chrome + firefox
     * development w/ new CSS features like RTL, mask, ...
     */

    innerWebpackConfig.module.rules.push({
      test: /svg$/,
      loader: "svg-inline-loader",
    });

    const cssUses = [
      {
        loader: "style-loader",
      },
      {
        loader: "css-loader",
        options: {
          importLoaders: 1,
          url: false,
        },
      },
    ];

    if (!options.disablePostCSS) {
      cssUses.push({ loader: "postcss-loader" });
    }

    innerWebpackConfig.module.rules.push({
      test: /\.css$/,
      use: cssUses,
    });

    innerWebpackConfig.plugins.push(
      new webpack.NormalModuleReplacementPlugin(
        /(resource:|chrome:).*.css/,
        function(resource) {
          const newUrl = resource.request
            .replace(
              /(.\/chrome:\/\/|.\/resource:\/\/)/,
              `devtools-mc-assets/assets/`
            )
            .replace(/devtools\/skin/, "devtools/client/themes")
            .replace(/devtools\/content/, "devtools/client");

          resource.request = newUrl;
        }
      )
    );
  } else {
    /*
     * SVGs are loaded in one of two ways in JS w/ SVG inline loader
     * and in CSS w/ the CSS loader.
     *
     * Inline SVGs are included in the JS bundle and mounted w/ React.
     *
     * SVG URLs like /images/arrow.svg are mapped
     * by the postcss-loader to chrome://chrome://devtools/skin/images/debugger/arrow.svg,
     * copied to devtools/themes/images/debugger and added to the jar.mn
     *
     * SVG URLS like chrome://devtools/skin/images/add.svg are
     * ignored as they are already available in MC.
     */

    const cssUses = [
      {
        loader: "css-loader",
        options: {
          importLoaders: 1,
          url: false,
        },
      },
    ];

    if (!options.disablePostCSS) {
      cssUses.push({ loader: "postcss-loader" });
    }

    // Extract CSS into a single file
    innerWebpackConfig.module.rules.push({
      test: /\.css$/,
      exclude: request => {
        // If the tool defines an exclude regexp for CSS files.
        return (
          innerWebpackConfig.cssExcludes &&
          request.match(innerWebpackConfig.cssExcludes)
        );
      },
      use: ExtractTextPlugin.extract({
        filename: "*.css",
        use: cssUses,
      }),
    });

    innerWebpackConfig.module.rules.push({
      test: /svg$/,
      loader: "svg-inline-loader",
    });

    innerWebpackConfig.plugins.push(new ExtractTextPlugin("[name].css"));
  }

  if (isFirefoxPanel()) {
    innerWebpackConfig = devtoolsConfig(
      innerWebpackConfig,
      innerEnvConfig,
      options
    );
  }

  // NOTE: This is only needed to fix a bug with chrome devtools' debugger and
  // destructuring params https://github.com/firefox-devtools/debugger.html/issues/67
  if (getValue("transformParameters")) {
    innerWebpackConfig.module.rules.forEach(spec => {
      if (spec.isJavaScriptLoader) {
        const idx = spec.rules.findIndex(loader =>
          loader.includes("babel-loader")
        );
        spec.rules[idx] += "&plugins[]=transform-es2015-parameters";
      }
    });
  }

  return innerWebpackConfig;
}

function devtoolsConfig(innerWebpackConfig, innerEnvConfig, options) {
  const nativeMapping = {
    react: "devtools/client/shared/vendor/react",
    "react-dom": "devtools/client/shared/vendor/react-dom",
    "react-dom-factories": "devtools/client/shared/vendor/react-dom-factories",
    "prop-types": "devtools/client/shared/vendor/react-prop-types",
    lodash: "devtools/client/shared/vendor/lodash",
  };

  const outputPath = process.env.OUTPUT_PATH;

  if (outputPath) {
    innerWebpackConfig.output.path = outputPath;
  }

  innerWebpackConfig.devtool = false;
  const recordsPath = options.recordsPath || "assets/module-manifest.json";
  innerWebpackConfig.recordsPath = path.join(__dirname, recordsPath);

  function externalsTest(context, request, callback) {
    const mod = request;

    if (mod.match(/.*\/Services$/)) {
      callback(null, "Services");
      return;
    }

    // Any matching paths here won't be included in the bundle.
    const excludeMap = (options && options.excludeMap) || nativeMapping;
    if (excludeMap[mod]) {
      const mapping = excludeMap[mod];

      if (innerWebpackConfig.externalsRequire) {
        // If the tool defines "externalsRequire" in the webpack config, wrap
        // all require to external dependencies with a call to the tool's
        // externalRequire method.
        const reqMethod = innerWebpackConfig.externalsRequire;
        callback(null, `var ${reqMethod}("${mapping}")`);
      } else {
        callback(null, mapping);
      }
      return;
    }
    callback();
  }

  innerWebpackConfig.externals = innerWebpackConfig.externals || [];
  innerWebpackConfig.externals.push(externalsTest);

  // Remove the existing DefinePlugin so we can override it.
  const plugins = innerWebpackConfig.plugins.filter(
    p => !(p instanceof webpack.DefinePlugin)
  );
  innerWebpackConfig.plugins = plugins.concat([
    new webpack.DefinePlugin({
      "process.env": {
        NODE_ENV: JSON.stringify(process.env.NODE_ENV || "production"),
        TARGET: JSON.stringify("firefox-panel"),
      },
      DebuggerConfig: JSON.stringify(innerEnvConfig),
    }),
  ]);

  const mappings = [
    [/.\/src\/network-request/, "./src/privileged-network-request"],
  ];

  mappings.forEach(([regex, res]) => {
    innerWebpackConfig.plugins.push(
      new webpack.NormalModuleReplacementPlugin(regex, res)
    );
  });

  return innerWebpackConfig;
}
