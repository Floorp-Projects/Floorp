/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

const { dispatcher } = require("sdk/util/dispatcher");

exports["test dispatcher API"] = assert => {
  const dispatch = dispatcher();

  assert.equal(typeof(dispatch), "function",
               "dispatch is a function");

  assert.equal(typeof(dispatch.define), "function",
               "dispatch.define is a function");

  assert.equal(typeof(dispatch.implement), "function",
               "dispatch.implement is a function");

  assert.equal(typeof(dispatch.when), "function",
               "dispatch.when is a function");
};

exports["test dispatcher"] = assert => {
  const isDuck = dispatcher();

  const quacks = x => x && typeof(x.quack) === "function";

  const Duck = function() {};
  const Goose = function() {};

  const True = _ => true;
  const False = _ => false;



  isDuck.define(Goose, False);
  isDuck.define(Duck, True);
  isDuck.when(quacks, True);

  assert.equal(isDuck(new Goose()), false,
               "Goose ain't duck");

  assert.equal(isDuck(new Duck()), true,
               "Ducks are ducks");

  assert.equal(isDuck({ quack: () => "Quaaaaaack!" }), true,
               "It's a duck if it quacks");


  assert.throws(() => isDuck({}), /Type does not implements method/, "not implemneted");

  isDuck.define(Object, False);

  assert.equal(isDuck({}), false,
               "Ain't duck if it does not quacks!");
};

exports["test redefining fails"] = assert => {
  const isPM = dispatcher();
  const isAfternoon = time => time.getHours() > 12;

  isPM.when(isAfternoon, _ => true);

  assert.equal(isPM(new Date(Date.parse("Jan 23, 1985, 13:20:00"))), true,
               "yeap afternoon");
  assert.equal(isPM({ getHours: _ => 17 }), true,
                "seems like afternoon");

  assert.throws(() => isPM.when(isAfternoon, x => x > 12 && x < 24),
                /Already implemented for the given predicate/,
               "can't redefine on same predicate");

};

require("sdk/test").run(exports);
