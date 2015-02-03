/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");

const toolkit = require("toolkit/require");

const {tmpdir} = require("node/os");
const {join} = require("sdk/fs/path");
const {writeFile, unlink} = require("sdk/io/fs");
const {fromFilename} = require("sdk/url");

const makeCallback = (resolve, reject) => (error, result) => {
  if (error) reject(error);
  else resolve(result);
};

const remove = path => new Promise((resolve, reject) =>
  unlink(path, makeCallback(resolve, reject)));

const write = (...params) => new Promise((resolve, reject) =>
  writeFile(...params, makeCallback(resolve, reject)));

exports.testReload = function*(assert) {
  const modulePath = join(tmpdir(), "toolkit-require-reload.js");
  const moduleURI = fromFilename(modulePath);

  yield write(modulePath, `exports.version = () => 1;`);

  const v1 = toolkit.require(moduleURI);

  assert.equal(v1.version(), 1, "module exports version");

  yield write(modulePath, `exports.version = () => 2;`);

  assert.equal(v1, toolkit.require(moduleURI),
               "require does not reload modules");

  const v2 = toolkit.require(moduleURI, {reload: true});
  assert.equal(v2.version(), 2, "module was updated");

  yield remove(modulePath);
};

exports.testReloadAll = function*(assert) {
  const parentPath = join(tmpdir(), "toolkit-require-reload-parent.js");
  const childPath = join(tmpdir(), "toolkit-require-reload-child.js");

  const parentURI = fromFilename(parentPath);
  const childURI = fromFilename(childPath);

  yield write(childPath, `exports.name = () => "child"`);
  yield write(parentPath, `const child = require("./toolkit-require-reload-child");
                           exports.greet = () => "Hello " + child.name();`);

  const parent1 = toolkit.require(parentURI);
  assert.equal(parent1.greet(), "Hello child");

  yield write(childPath, `exports.name = () => "father"`);
  yield write(parentPath, `const child = require("./toolkit-require-reload-child");
                           exports.greet = () => "Hello " + child.name() + "!";`);

  const parent2 = toolkit.require(parentURI, {reload: true});
  assert.equal(parent2.greet(), "Hello child!",
               "only parent changes were picked up");

  const parent3 = toolkit.require(parentURI, {reload: true, all: true});
  assert.equal(parent3.greet(), "Hello father!",
               "all changes were picked up");

  yield remove(childPath);
  yield remove(parentPath);
};

exports.main = _ => require("sdk/test/runner").runTestsFromModule(module);
