/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function empties() {
  const a = "";
  const b = false;
  const c = undefined;
  const d = null;
  debugger;
}

function smalls() {
  const a = "...";
  const b = true;
  const c = 1;
  const d = [];
  const e = {};
  debugger;
}

function objects() {
  const obj = {
    foo: 1,
  };

  const empty = Object.create(null);
  const foo = obj?.foo;

  debugger;
}

function largeArray() {
  const bs = [];
  for (let i = 0; i <= 100; i++) {
    bs.push({ a: 2, b: { c: 3 } });
  }
  debugger;
}

function classPreview() {
  class Foo {
    x = 1;
    #privateVar = 2;
    #privateMethod() {
      return this.#privateVar;
    }
    breakFn() {
      let i = this.x * this.#privateVar;
      debugger;
    }
  }
  const foo = new Foo();
  foo.breakFn();
}
