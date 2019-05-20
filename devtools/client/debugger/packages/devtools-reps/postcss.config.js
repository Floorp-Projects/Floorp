/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const mapUrl = require("postcss-url-mapper");
const MC_PATH = "resource://devtools/client/shared/components/reps/images/";
const EXPRESS_PATH = "/devtools-reps/images/";
const IMAGES = ["open-inspector.svg", "jump-definition.svg", "input.svg"];

function mapUrlProduction(url, type) {
  for (const img of IMAGES) {
    url = url.replace(`^/images/${img}`, `${MC_PATH}${img}`);
  }
  return url;
}

function mapUrlDevelopment(url) {
  for (const img of IMAGES) {
    url = url.replace(`^/images/${img}`, `${EXPRESS_PATH}${img}`);
  }
  return url;
}

module.exports = ({ file, options, env }) => {
  if (env === "production") {
    return {
      plugins: [mapUrl(mapUrlProduction)],
    };
  }

  return {
    plugins: [mapUrl(mapUrlDevelopment)],
  };
};
