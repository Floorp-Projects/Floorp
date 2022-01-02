/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const path = require("path");
const _ = require("lodash");
const rollup = require("rollup");
const rollupTypescript = require("rollup-plugin-typescript2");

const TARGET_NAME = "rollup";

module.exports = exports = async function(tests, dirname) {
  const fixtures = [];
  for (const [name, input] of tests) {
    if (/babel-|-cjs/.test(name)) {
      continue;
    }

    const testFnName = _.camelCase(`${TARGET_NAME}-${name}`);

    const scriptPath = path.join(dirname, "output", TARGET_NAME, `${name}.js`);

    console.log(`Building ${TARGET_NAME} test ${name}`);

    const bundle = await rollup.rollup({
      input: "fake-bundle-root",
      plugins: [
        // Our input file may export more than the default, but we
        // want to enable 'exports: "default",' so we need the root
        // import to only have a default export.
        {
          resolveId: id => (id === "fake-bundle-root" ? id : undefined),
          load: id =>
            id === "fake-bundle-root"
              ? `import test from "${input}"; export default test;`
              : undefined
        },
        {
          ongenerate(out, data) {
            data.map.sources = data.map.sources.map(source =>
              source.replace(/^fixtures[\\/]/, `${TARGET_NAME}://./`)
            );
          }
        },
        rollupTypescript({
          check: false,
          tsconfigDefaults: {
            target: "es6",
          },
        }),
      ].filter(Boolean)
    });

    await bundle.write({
      file: path.basename(scriptPath),
      dir: path.dirname(scriptPath),
      format: "iife",
      name: testFnName,
      // sourceMapFile: ""
      sourcemap: true,
      exports: "default"
    });

    fixtures.push({
      name,
      testFnName,
      scriptPath,
      assets: [scriptPath, `${scriptPath}.map`]
    });
  }

  return {
    target: TARGET_NAME,
    fixtures
  };
};
