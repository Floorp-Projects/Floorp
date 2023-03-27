
const path = require("path");

module.exports = [{
    mode: "production",
    entry: `./node_modules/micromatch/index.js`,
    optimization: {
        minimize: false
    },
    output: {
      path: __dirname,
      filename: `micromatch.js`,
      library: "micromatch",
      libraryTarget: "commonjs"
    },
    node: {
      util: true,
      path: true
    }
}];