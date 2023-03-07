const path = require("path");

module.exports = [1, 2].map(version => {
  return {
    devtool: "source-map",
    mode: "development",
    entry: [path.join(__dirname, `/v${version}/code_reload_${version}.js`)],
    output: {
      path: __dirname,
      filename: `v${version}/code_bundle_reload.js`,
    },
  };
});
