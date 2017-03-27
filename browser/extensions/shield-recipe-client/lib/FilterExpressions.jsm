/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://shield-recipe-client/lib/Sampling.jsm");

this.EXPORTED_SYMBOLS = ["FilterExpressions"];

XPCOMUtils.defineLazyGetter(this, "nodeRequire", () => {
  const {Loader, Require} = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
  const loader = new Loader({
    paths: {
      "": "resource://shield-recipe-client/node_modules/",
    },
  });
  return new Require(loader, {});
});

XPCOMUtils.defineLazyGetter(this, "jexl", () => {
  const {Jexl} = nodeRequire("jexl/lib/Jexl.js");
  const jexl = new Jexl();
  jexl.addTransforms({
    date: dateString => new Date(dateString),
    stableSample: Sampling.stableSample,
    bucketSample: Sampling.bucketSample,
  });
  return jexl;
});

this.FilterExpressions = {
  eval(expr, context = {}) {
    const onelineExpr = expr.replace(/[\t\n\r]/g, " ");
    return jexl.eval(onelineExpr, context);
  },
};
