/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { isWeak, WeakReference } = require("sdk/core/reference");
const { Class } = require("sdk/core/heritage");

exports["test non WeakReferences"] = assert => {
  assert.equal(isWeak({}), false,
               "regular objects aren't weak");

  assert.equal(isWeak([]), false,
               "arrays aren't weak");
};

exports["test a WeakReference"] = assert => {
  assert.equal(isWeak(new WeakReference()), true,
               "instances of WeakReferences are weak");
};

exports["test a subclass"] = assert => {
  const SubType = Class({
    extends: WeakReference,
    foo: function() {
    }
  });

  const x = new SubType();
  assert.equal(isWeak(x), true,
               "subtypes of WeakReferences are weak");

  const SubSubType = Class({
    extends: SubType
  });

  const y = new SubSubType();
  assert.equal(isWeak(y), true,
               "sub subtypes of WeakReferences are weak");
};

exports["test a mixin"] = assert => {
  const Mixer = Class({
    implements: [WeakReference]
  });

  assert.equal(isWeak(new Mixer()), true,
               "instances with mixed WeakReference are weak");

  const Mux = Class({});
  const MixMux = Class({
    extends: Mux,
    implements: [WeakReference]
  });

  assert.equal(isWeak(new MixMux()), true,
               "instances with mixed WeakReference that " +
               "inheret from other are weak");


  const Base = Class({});
  const ReMix = Class({
    extends: Base,
    implements: [MixMux]
  });

  assert.equal(isWeak(new ReMix()), true,
               "subtime of mix is still weak");
};

exports["test an override"] = assert => {
  const SubType = Class({ extends: WeakReference });
  isWeak.define(SubType, x => false);

  assert.equal(isWeak(new SubType()), false,
               "behavior of isWeak can still be overrided on subtype");

  const Mixer = Class({ implements: [WeakReference] });
  const SubMixer = Class({ extends: Mixer });
  isWeak.define(SubMixer, x => false);
  assert.equal(isWeak(new SubMixer()), false,
               "behavior of isWeak can still be overrided on mixed type");
};

exports["test an opt-in"] = assert => {
  var BaseType = Class({});
  var SubType = Class({ extends: BaseType });

  isWeak.define(SubType, x => true);
  assert.equal(isWeak(new SubType()), true,
               "isWeak can be just implemented");

  var x = {};
  isWeak.implement(x, x => true);
  assert.equal(isWeak(x), true,
               "isWeak can be implemented per instance");
};

require("sdk/test").run(exports);
