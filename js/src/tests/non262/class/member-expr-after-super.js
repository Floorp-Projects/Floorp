/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class Base {
    get test() {
        return "ok";
    }
}

class SuperThenProperty extends Base {
    constructor() {
        var result = super().test;
        return {result};
    }
}
assertEq(new SuperThenProperty().result, "ok");

class SuperThenMember extends Base {
    constructor() {
        var result = super()["tes" + String.fromCodePoint("t".codePointAt(0))];
        return {result};
    }
}
assertEq(new SuperThenMember().result, "ok");

class SuperThenCall extends Function {
    constructor() {
        var result = super("o, k", "return o + k;")("o", "k");
        return {result};
    }
}
assertEq(new SuperThenCall().result, "ok");

class SuperThenTemplateCall extends Function {
    constructor() {
        var result = super("cooked", "return cooked[0][0] + cooked.raw[0][1];")`ok`;
        return {result};
    }
}
assertEq(new SuperThenTemplateCall().result, "ok");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
