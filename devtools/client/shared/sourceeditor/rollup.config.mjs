/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable import/no-unresolved */
import terser from "@rollup/plugin-terser";
import nodeResolve from "@rollup/plugin-node-resolve";

export default function (commandLineArgs) {
  const plugins = [nodeResolve()];
  if (commandLineArgs.minified) {
    plugins.push(terser());
  }

  return {
    input: "codemirror6/index.mjs",
    output: {
      file: "codemirror6/codemirror6.bundle.mjs",
      format: "esm",
    },
    plugins,
  };
}
