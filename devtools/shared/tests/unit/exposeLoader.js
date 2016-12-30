/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

exports.exerciseLazyRequire = (name, path) => {
  const o = {};
  loader.lazyRequireGetter(o, name, path);
  return o;
};
