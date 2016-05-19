/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Loader } = require("sdk/test/loader");
const { Class } = require("sdk/core/heritage");
const { Disposable } = require("sdk/core/disposable");
const { Cc, Ci, Cu } = require("chrome");
const { setTimeout } = require("sdk/timers");

exports["test disposeDisposable"] = assert => {
  let loader = Loader(module);

  const { Disposable, disposeDisposable } = loader.require("sdk/core/disposable");
  const { isWeak, WeakReference } = loader.require("sdk/core/reference");

  let disposals = 0;

  const Foo = Class({
    extends: Disposable,
    implements: [WeakReference],
    dispose(...params) {
      disposeDisposable(this);
      disposals = disposals + 1;
    }
  });

  const f1 = new Foo();
  assert.equal(isWeak(f1), true, "f1 has WeakReference support");

  f1.dispose();
  assert.equal(disposals, 1, "disposed on dispose");

  loader.unload("uninstall");
  assert.equal(disposals, 1, "after disposeDisposable, dispose is not called anymore");
};

exports["test destroy reasons"] = assert => {
  let disposals = 0;

  const Foo = Class({
    extends: Disposable,
    dispose: function() {
      disposals = disposals + 1;
    }
  });

  const f1 = new Foo();

  f1.destroy();
  assert.equal(disposals, 1, "disposed on destroy");
  f1.destroy();
  assert.equal(disposals, 1, "second destroy is ignored");

  disposals = 0;
  const f2 = new Foo();

  f2.destroy("uninstall");
  assert.equal(disposals, 1, "uninstall invokes disposal");
  f2.destroy("uninstall")
  f2.destroy();
  assert.equal(disposals, 1, "disposal happens just once");

  disposals = 0;
  const f3 = new Foo();

  f3.destroy("shutdown");
  assert.equal(disposals, 0, "shutdown invoke disposal");
  f3.destroy("shutdown");
  f3.destroy();
  assert.equal(disposals, 0, "shutdown disposal happens just once");

  disposals = 0;
  const f4 = new Foo();

  f4.destroy("disable");
  assert.equal(disposals, 1, "disable invokes disposal");
  f4.destroy("disable")
  f4.destroy();
  assert.equal(disposals, 1, "destroy happens just once");

  disposals = 0;
  const f5 = new Foo();

  f5.destroy("disable");
  assert.equal(disposals, 1, "disable invokes disposal");
  f5.destroy("disable")
  f5.destroy();
  assert.equal(disposals, 1, "destroy happens just once");

  disposals = 0;
  const f6 = new Foo();

  f6.destroy("upgrade");
  assert.equal(disposals, 1, "upgrade invokes disposal");
  f6.destroy("upgrade")
  f6.destroy();
  assert.equal(disposals, 1, "destroy happens just once");

  disposals = 0;
  const f7 = new Foo();

  f7.destroy("downgrade");
  assert.equal(disposals, 1, "downgrade invokes disposal");
  f7.destroy("downgrade")
  f7.destroy();
  assert.equal(disposals, 1, "destroy happens just once");


  disposals = 0;
  const f8 = new Foo();

  f8.destroy("whatever");
  assert.equal(disposals, 1, "unrecognized reason invokes disposal");
  f8.destroy("meh")
  f8.destroy();
  assert.equal(disposals, 1, "destroy happens just once");
};

exports["test different unload hooks"] = assert => {
  const { uninstall, shutdown, disable, upgrade,
          downgrade, dispose } = require("sdk/core/disposable");
  const UberUnload = Class({
    extends: Disposable,
    setup: function() {
      this.log = [];
    }
  });

  uninstall.define(UberUnload, x => x.log.push("uninstall"));
  shutdown.define(UberUnload, x => x.log.push("shutdown"));
  disable.define(UberUnload, x => x.log.push("disable"));
  upgrade.define(UberUnload, x => x.log.push("upgrade"));
  downgrade.define(UberUnload, x => x.log.push("downgrade"));
  dispose.define(UberUnload, x => x.log.push("dispose"));

  const u1 = new UberUnload();
  u1.destroy("uninstall");
  u1.destroy();
  u1.destroy("shutdown");
  assert.deepEqual(u1.log, ["uninstall"], "uninstall hook invoked");

  const u2 = new UberUnload();
  u2.destroy("shutdown");
  u2.destroy();
  u2.destroy("uninstall");
  assert.deepEqual(u2.log, ["shutdown"], "shutdown hook invoked");

  const u3 = new UberUnload();
  u3.destroy("disable");
  u3.destroy();
  u3.destroy("uninstall");
  assert.deepEqual(u3.log, ["disable"], "disable hook invoked");

  const u4 = new UberUnload();
  u4.destroy("upgrade");
  u4.destroy();
  u4.destroy("uninstall");
  assert.deepEqual(u4.log, ["upgrade"], "upgrade hook invoked");

  const u5 = new UberUnload();
  u5.destroy("downgrade");
  u5.destroy();
  u5.destroy("uninstall");
  assert.deepEqual(u5.log, ["downgrade"], "downgrade hook invoked");

  const u6 = new UberUnload();
  u6.destroy();
  u6.destroy();
  u6.destroy("uninstall");
  assert.deepEqual(u6.log, ["dispose"], "dispose hook invoked");

  const u7 = new UberUnload();
  u7.destroy("whatever");
  u7.destroy();
  u7.destroy("uninstall");
  assert.deepEqual(u7.log, ["dispose"], "dispose hook invoked");
};

exports["test disposables are disposed on unload"] = function(assert) {
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
  let { WeakReference } = loader.require("sdk/core/reference");

  let arg1 = {}
  let arg2 = 2
  let disposals = 0

  let Foo = Class({
    extends: Disposable,
    implements: [WeakReference],
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
  });

  let foo1 = Foo(arg1, arg2)
  let foo2 = Foo(arg1, arg2)

  foo1 = foo2 = null;

  Cu.schedulePreciseGC(function() {
    loader.unload();
    assert.equal(disposals, 0, "GC removed dispose listeners");
    done();
  });
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

exports["test multiple destroy"] = function(assert) {
  let loader = Loader(module);
  let { Disposable } = loader.require("sdk/core/disposable");

  let disposals = 0

  let Foo = Class({
    extends: Disposable,
    dispose: function dispose() {
      disposals = disposals + 1
    }
  })

  let foo1 = Foo();
  let foo2 = Foo();
  let foo3 = Foo();

  assert.equal(disposals, 0, "no disposals yet");

  foo1.destroy();
  assert.equal(disposals, 1, "disposed properly");
  foo1.destroy();
  assert.equal(disposals, 1, "didn't attempt to dispose twice");

  foo2.destroy();
  assert.equal(disposals, 2, "other instances still dispose fine");
  foo2.destroy();
  assert.equal(disposals, 2, "but not twice");

  loader.unload();

  assert.equal(disposals, 3, "unload only disposed the remaining instance");
}

require('sdk/test').run(exports);
