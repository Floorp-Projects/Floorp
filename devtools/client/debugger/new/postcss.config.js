/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

var mapUrl = require("postcss-url-mapper");
const debug = require("debug")("launchpad");

function mapUrlProduction(url) {
  const newUrl = url.replace(
    /\/images\//,
    "resource://devtools/client/debugger/new/images/"
  );

  debug("map url", { url, newUrl });
  return newUrl;
}

function mapUrlDevelopment(url) {
  const newUrl = url
    .replace(/(chrome:\/\/|resource:\/\/)/, "/mc/")
    .replace(/devtools\/skin/, "devtools/client/themes")
    .replace(/devtools\/content/, "devtools/client");

  debug("map url", { url, newUrl });
  return newUrl;
}

module.exports = ({ file, options, env }) => {
  if (env === "production") {
    return {
      plugins: [mapUrl(mapUrlProduction)]
    };
  }

  return {
    plugins: [
      require("postcss-bidirection"),
      require("autoprefixer")({
        browsers: ["last 2 Firefox versions", "last 2 Chrome versions"],
        flexbox: false,
        grid: false
      }),
      require("postcss-class-namespace")(),
      mapUrl(mapUrlDevelopment)
    ]
  };
};
