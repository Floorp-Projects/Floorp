/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const method = require("method/core");

// Utility function that is just an enhancement over `method` to
// allow predicate based dispatch in addition to polymorphic
// dispatch. Unfortunately polymorphic dispatch does not quite
// cuts it in the world of XPCOM where no types / classes exist
// and all the XUL nodes share same type / prototype.
// Probably this is more generic and belongs some place else, but
// we can move it later once this will be relevant.
var dispatcher = hint => {
  const base = method(hint);
  // Make a map for storing predicate, implementation mappings.
  let implementations = new Map();

  // Dispatcher function goes through `predicate, implementation`
  // pairs to find predicate that matches first argument and
  // returns application of arguments on the associated
  // `implementation`. If no matching predicate is found delegates
  // to a `base` polymorphic function.
  let dispatch = (value, ...rest) => {
    for (let [predicate, implementation] of implementations) {
      if (predicate(value))
        return implementation(value, ...rest);
    }

    return base(value, ...rest);
  };

  // Expose base API.
  dispatch.define = base.define;
  dispatch.implement = base.implement;
  dispatch.toString = base.toString;

  // Add a `when` function to allow extending function via
  // predicates.
  dispatch.when = (predicate, implementation) => {
    if (implementations.has(predicate))
      throw TypeError("Already implemented for the given predicate");
    implementations.set(predicate, implementation);
  };

  return dispatch;
};

exports.dispatcher = dispatcher;
