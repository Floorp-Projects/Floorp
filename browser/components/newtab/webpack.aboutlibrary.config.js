const path = require("path");
const config = require("./webpack.system-addon.config.js");
const absolute = relPath => path.join(__dirname, relPath);
module.exports = Object.assign({}, config, {
  entry: absolute("content-src/aboutlibrary/aboutlibrary.jsx"),
  output: {
    path: absolute("aboutlibrary/content"),
    filename: "aboutlibrary.bundle.js",
  },
});
