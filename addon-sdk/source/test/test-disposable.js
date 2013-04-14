/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Loader } = require("sdk/test/loader");
const { Class } = require("sdk/core/heritage");
const { Cc, Ci, Cu } = require("chrome");
const { setTimeout } = require("sdk/timers");

exports["test disposables are desposed on unload"] = function(assert) {
  let loader = Loader(module);
  let { Disposable } = loader.require("sdk/core/disposable");

  let arg1 = {}
  let arg2 = 2
  let disposals = 0

  let Foo = Class({
    extends: Disposable,
    setup: function setup(a, b) {
      assert.equal(a, arg1,
                   "arguments passed to constructur is passed to setup");
      assert.equal(b, arg2,
                   "second argument is also passed to a setup");
      assert.ok(this instanceof Foo,
                "this is an instance in the scope of the setup method");

      this.fooed = true
    },
    dispose: function dispose() {
      assert.equal(this.fooed, true, "attribute was set")
      this.fooed = false
      disposals = disposals + 1
    }
  })

  let foo1 = Foo(arg1, arg2)
  let foo2 = Foo(arg1, arg2)

  loader.unload();

  assert.equal(disposals, 2, "both instances were disposed")
}

exports["test destroyed windows dispose before unload"] = function(assert) {
  let loader = Loader(module);
  let { Disposable } = loader.require("sdk/core/disposable");

  let arg1 = {}
  let arg2 = 2
  let disposals = 0

  let Foo = Class({
    extends: Disposable,
    setup: function setup(a, b) {
      assert.equal(a, arg1,
                   "arguments passed to constructur is passed to setup");
      assert.equal(b, arg2,
                   "second argument is also passed to a setup");
      assert.ok(this instanceof Foo,
                "this is an instance in the scope of the setup method");

      this.fooed = true
    },
    dispose: function dispose() {
      assert.equal(this.fooed, true, "attribute was set")
      this.fooed = false
      disposals = disposals + 1
    }
  })

  let foo1 = Foo(arg1, arg2)
  let foo2 = Foo(arg1, arg2)

  foo1.destroy();
  assert.equal(disposals, 1, "destroy disposes instance")

  loader.unload();

  assert.equal(disposals, 2, "unload disposes alive instances")
}

exports["test disposables are GC-able"] = function(assert, done) {
  let loader = Loader(module);
  let { Disposable } = loader.require("sdk/core/disposable");

  let arg1 = {}
  let arg2 = 2
  let disposals = 0

  let Foo = Class({
    extends: Disposable,
    setup: function setup(a, b) {
      assert.equal(a, arg1,
                   "arguments passed to constructur is passed to setup");
      assert.equal(b, arg2,
                   "second argument is also passed to a setup");
      assert.ok(this instanceof Foo,
                "this is an instance in the scope of the setup method");

      this.fooed = true
    },
    dispose: function dispose() {
      assert.equal(this.fooed, true, "attribute was set")
      this.fooed = false
      disposals = disposals + 1
    }
  })

  let foo1 = Foo(arg1, arg2)
  let foo2 = Foo(arg1, arg2)

  let foo1 = null
  let foo2 = null

  Cu.forceGC();
  setTimeout(function() {
    Cu.forceGC();
    loader.unload();
    assert.equal(disposals, 0, "GC removed dispose listeners");
    done();
  }, 300);
}


exports["test loader unloads do not affect other loaders"] = function(assert) {
  let loader1 = Loader(module);
  let loader2 = Loader(module);
  let { Disposable: Disposable1 } = loader1.require("sdk/core/disposable");
  let { Disposable: Disposable2 } = loader2.require("sdk/core/disposable");

  let arg1 = {}
  let arg2 = 2
  let disposals = 0

  let Foo1 = Class({
    extends: Disposable1,
    dispose: function dispose() {
      disposals = disposals + 1;
    }
  });

  let Foo2 = Class({
    extends: Disposable2,
    dispose: function dispose() {
      disposals = disposals + 1;
    }
  });

  let foo1 = Foo1(arg1, arg2);
  let foo2 = Foo2(arg1, arg2);

  assert.equal(disposals, 0, "no destroy calls");

  loader1.unload();

  assert.equal(disposals, 1, "1 destroy calls");

  loader2.unload();

  assert.equal(disposals, 2, "2 destroy calls");
}

exports["test disposables that throw"] = function(assert) {
  let loader = Loader(module);
  let { Disposable } = loader.require("sdk/core/disposable");

  let disposals = 0

  let Foo = Class({
    extends: Disposable,
    setup: function setup(a, b) {
      throw Error("Boom!")
    },
    dispose: function dispose() {
      disposals = disposals + 1
    }
  })

  assert.throws(function() {
    let foo1 = Foo()
  }, /Boom/, "disposable constructors may throw");

  loader.unload();

  assert.equal(disposals, 0, "no disposal if constructor threw");
}

require('test').run(exports);
