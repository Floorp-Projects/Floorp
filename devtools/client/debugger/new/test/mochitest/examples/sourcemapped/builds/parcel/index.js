/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const path = require("path");
const util = require("util");
const fs = require("fs");
const _ = require("lodash");
const Bundler = require("parcel-bundler");
const convertSourceMap = require("convert-source-map");

const TARGET_NAME = "parcel";

module.exports = exports = async function(tests, dirname) {
  const fixtures = [];
  for (const [name, input] of tests) {
    const testFnName = _.camelCase(`${TARGET_NAME}-${name}`);
    const evalMaps = name.match(/-eval/);

    console.log(`Building ${TARGET_NAME} test ${name}`);

    const scriptPath = path.join(dirname, "output", TARGET_NAME, `${name}.js`);
    const bundler = new Bundler(input, {
      global: testFnName,
      hmr: false,
      watch: false,
      logLevel: 0,
      publicURL: ".",

      outDir: path.dirname(scriptPath),
      outFile: path.basename(scriptPath),
    });

    await bundler.bundle();

    let code = fs.readFileSync(scriptPath, "utf8");

    // Parcel doesn't offer a way to explicitly export the default property
    // of the default object.
    code += `\n;${testFnName} = ${testFnName}.default;`;
    fs.writeFileSync(scriptPath, code);


    const mapMatch = convertSourceMap.mapFileCommentRegex.exec(code);
    const mapPath = path.resolve(path.dirname(scriptPath), mapMatch[1] || mapMatch[2]);

    const map = JSON.parse(fs.readFileSync(mapPath, "utf8"));
    map.sourceRoot = undefined;
    map.sources = map.sources.map(source =>
      source.replace(/^/, `${TARGET_NAME}://./${name}/`)
    );
    fs.writeFileSync(mapPath, JSON.stringify(map));

    fixtures.push({
      name,
      testFnName,
      scriptPath,
      assets: [
        scriptPath,
        `${path.join(path.dirname(scriptPath), path.basename(scriptPath, path.extname(scriptPath)))}.map`,
      ]
    });
  }

  return {
    target: TARGET_NAME,
    fixtures
  };
};
