/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

var mapUrl = require("postcss-url-mapper");
const debug = require("debug")("launchpad");

function mapUrlDevelopment(url) {
  const newUrl = url
    .replace(/resource:\/\/devtools\/client\/debugger\/images\//, "/images/")
    .replace(/(chrome:\/\/)/, "/images/")
    .replace(/devtools\/skin/, "devtools/client/themes")
    .replace(/devtools\/content/, "devtools/client");

  debug("map url", { url, newUrl });
  return newUrl;
}

module.exports = ({ file, options, env }) => {
  if (env === "production") {
    return {};
  }

  return {
    plugins: [
      require("autoprefixer")({
        browsers: ["last 2 Firefox versions", "last 2 Chrome versions"],
        flexbox: false,
        grid: false,
      }),
      require("postcss-class-namespace")(),
      mapUrl(mapUrlDevelopment),
    ],
  };
};
