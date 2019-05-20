/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const mapUrl = require("postcss-url-mapper");
const MC_PATH = "resource://devtools/client/debugger/images/";
const EXPRESS_PATH = "/devtools-components/images/";

function mapUrlProduction(url, type) {
  return url.replace("/images/arrow.svg", `${MC_PATH}arrow.svg`);
}

function mapUrlDevelopment(url) {
  return url.replace("/images/arrow.svg", `${EXPRESS_PATH}arrow.svg`);
}

module.exports = ({ file, options, env }) => {
  // Here we don't want to do anything for storybook since we serve the images thanks
  // to the `-s ./src` option in the `storybook` command (see package.json).
  if (env === "storybook") {
    return {};
  }

  // This will be used when creating a bundle for mozilla-central (from devtools-reps
  // or debugger.html).
  if (env === "production") {
    return {
      plugins: [mapUrl(mapUrlProduction)],
    };
  }

  // This will be used when using this module in launchpad mode. We set a unique path so
  // we can serve images from express.
  return {
    plugins: [mapUrl(mapUrlDevelopment)],
  };
};
