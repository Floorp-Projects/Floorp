/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var test = `

var Promise = ShellPromise;

class X {
  constructor() {
    this.value = 42;
  }
  async getValue() {
    return this.value;
  }
  setValue(value) {
    this.value = value;
  }
  async increment() {
    var value = await this.getValue();
    this.setValue(value + 1);
    return this.getValue();
  }
  async getBaseClassName() {
    return 'X';
  }
  static async getStaticValue() {
    return 44;
  }
}

class Y extends X {
  constructor() { }
  async getBaseClassName() {
    return super.getBaseClassName();
  }
}

var objLiteral = {
  async get() {
    return 45;
  },
  someStuff: 5
};

var x = new X();
var y = new Y();

Promise.all([
  assertEventuallyEq(x.getValue(), 42),
  assertEventuallyEq(x.increment(), 43),
  assertEventuallyEq(X.getStaticValue(), 44),
  assertEventuallyEq(objLiteral.get(), 45),
  assertEventuallyEq(y.getBaseClassName(), 'X'),
]).then(() => {
  if (typeof reportCompare === "function")
      reportCompare(true, true);
});

`;

if (classesEnabled() && asyncFunctionsEnabled())
    eval(test);
else if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
