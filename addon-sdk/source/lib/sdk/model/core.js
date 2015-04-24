/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { dispatcher } = require("../util/dispatcher");


// Define `modelFor` accessor function that can be implemented
// for different types of views. Since view's we'll be dealing
// with types that don't really play well with `instanceof`
// operator we're gonig to use `dispatcher` that is slight
// extension over polymorphic dispatch provided by method.
// This allows models to extend implementations of this by
// providing predicates:
//
// modelFor.when($ => $ && $.nodeName === "tab", findTabById($.id))
const modelFor = dispatcher("modelFor");
exports.modelFor = modelFor;
