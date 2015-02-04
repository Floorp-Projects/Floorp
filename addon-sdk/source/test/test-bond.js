/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Bond } = require("sdk/util/bond");
const { Class } = require("sdk/core/heritage");

exports["test bonds on constructors"] = assert => {
  const MyClass = function(name) {
    this.name = name;
  }
  MyClass.prototype = Bond({
    hello() {
      return `Hello my name is ${this.name}`
    }
  })
  Object.assign(MyClass.prototype, {
    constructor: MyClass,
    readName() {
      return this.name
    }
  });

  const i1 = new MyClass("James Bond");

  assert.equal(i1.hello(), "Hello my name is James Bond");
  assert.equal(i1.hello.call({name: "Hack"}), "Hello my name is James Bond");
  assert.equal(i1.readName(), "James Bond");
  assert.equal(i1.readName.call({name: "Hack"}), "Hack");

  const hello = i1.hello
  assert.equal(hello(), "Hello my name is James Bond");
};

exports["test subclassing"] = assert => {
  const MyClass = function(name) {
    this.name = name;
  }
  MyClass.prototype = Bond({
    hello() {
      return `Hello my name is ${this.name}`
    }
  });

  const i1 = new MyClass("James Bond");

  assert.equal(i1.hello(), "Hello my name is James Bond");
  assert.equal(i1.hello.call({name: "Hack"}), "Hello my name is James Bond");
  const i1Hello = i1.hello
  assert.equal(i1Hello(), "Hello my name is James Bond");

  const MySubClass = function(...args) {
    MyClass.call(this, ...args)
  }
  MySubClass.prototype = Object.create(MyClass.prototype)

  const i2 = new MySubClass("Your father");

  assert.equal(i2.hello(), "Hello my name is Your father");
  assert.equal(i2.hello.call({name: "Hack"}), "Hello my name is Your father");
  const i2Hello = i2.hello
  assert.equal(i2Hello(), "Hello my name is Your father");
};

exports["test access on prototype"] = assert => {
  const MyClass = function(name) {
    this.name = name;
  }
  MyClass.prototype = Bond({
    hello() {
      return `Hello my name is ${this.name}`
    }
  });

  assert.equal(MyClass.prototype.hello(), "Hello my name is undefined");
  assert.ok(Object.getOwnPropertyDescriptor(MyClass.prototype, "hello").get,
            "hello is still a getter");
  assert.equal(MyClass.prototype.hello.call({name: "this"}),
               "Hello my name is this",
               "passing `this` on prototype methods work");

  const i1 = new MyClass("James Bond");
  assert.equal(i1.hello(), "Hello my name is James Bond");

  assert.ok(!Object.getOwnPropertyDescriptor(i1, "hello").get,
            "hello is not a getter on instance");

  assert.equal(i1.hello.call({name: "Hack"}), "Hello my name is James Bond");
  const i1Hello = i1.hello
  assert.equal(i1Hello(), "Hello my name is James Bond");
};


exports["test bonds with Class"] = assert => {
  const MyClass = Class({
    extends: Bond({
      hello() {
        return `Hello my name is ${this.name}`
      }
    }),
    initialize(name) {
      this.name = name;
    }
  });

  const i1 = new MyClass("James Bond");

  assert.equal(i1.hello(), "Hello my name is James Bond");
  assert.equal(i1.hello.call({name: "Hack"}), "Hello my name is James Bond");

  const hello = i1.hello
  assert.equal(hello(), "Hello my name is James Bond");
};


exports["test with mixin"] = assert => {
  const MyClass = Class({
    implements: [
      Bond({
        hello() {
          return `Hello my name is ${this.name}`
        }
      })
    ],
    initialize(name) {
      this.name = name;
    }
  });

  const i1 = new MyClass("James Bond");

  assert.equal(i1.hello(), "Hello my name is James Bond");
  assert.equal(i1.hello.call({name: "Hack"}), "Hello my name is James Bond");

  const hello = i1.hello
  assert.equal(hello(), "Hello my name is James Bond");

  const MyComposedClass = Class({
    implements: [
      MyClass,
      Bond({
        bye() {
          return `Bye ${this.name}`
        }
      })
    ],
    initialize(name) {
      this.name = name;
    }
  });

  const i2 = new MyComposedClass("Major Tom");

  assert.equal(i2.hello(), "Hello my name is Major Tom");
  assert.equal(i2.hello.call({name: "Hack"}), "Hello my name is Major Tom");

  const i2Hello = i2.hello
  assert.equal(i2Hello(), "Hello my name is Major Tom");

  assert.equal(i2.bye(), "Bye Major Tom");
  assert.equal(i2.bye.call({name: "Hack"}), "Bye Major Tom");

  const i2Bye = i2.bye
  assert.equal(i2Bye(), "Bye Major Tom");
};


require("sdk/test").run(exports);
