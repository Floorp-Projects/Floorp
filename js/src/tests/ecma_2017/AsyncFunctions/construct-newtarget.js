/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const AsyncFunction = async function(){}.constructor;


// Test subclassing %AsyncFunction% works correctly.
class MyAsync extends AsyncFunction {}

var fn = new MyAsync();
assertEq(fn instanceof MyAsync, true);
assertEq(fn instanceof AsyncFunction, true);
assertEq(Object.getPrototypeOf(fn), MyAsync.prototype);

fn = Reflect.construct(MyAsync, []);
assertEq(fn instanceof MyAsync, true);
assertEq(fn instanceof AsyncFunction, true);
assertEq(Object.getPrototypeOf(fn), MyAsync.prototype);

fn = Reflect.construct(MyAsync, [], MyAsync);
assertEq(fn instanceof MyAsync, true);
assertEq(fn instanceof AsyncFunction, true);
assertEq(Object.getPrototypeOf(fn), MyAsync.prototype);

fn = Reflect.construct(MyAsync, [], AsyncFunction);
assertEq(fn instanceof MyAsync, false);
assertEq(fn instanceof AsyncFunction, true);
assertEq(Object.getPrototypeOf(fn), AsyncFunction.prototype);


// Set a different constructor as NewTarget.
fn = Reflect.construct(MyAsync, [], Array);
assertEq(fn instanceof MyAsync, false);
assertEq(fn instanceof AsyncFunction, false);
assertEq(Object.getPrototypeOf(fn), Array.prototype);

fn = Reflect.construct(AsyncFunction, [], Array);
assertEq(fn instanceof AsyncFunction, false);
assertEq(Object.getPrototypeOf(fn), Array.prototype);


// The prototype defaults to %AsyncFunctionPrototype% if null.
function NewTargetNullPrototype() {}
NewTargetNullPrototype.prototype = null;

fn = Reflect.construct(AsyncFunction, [], NewTargetNullPrototype);
assertEq(fn instanceof AsyncFunction, true);
assertEq(Object.getPrototypeOf(fn), AsyncFunction.prototype);

fn = Reflect.construct(MyAsync, [], NewTargetNullPrototype);
assertEq(fn instanceof MyAsync, false);
assertEq(fn instanceof AsyncFunction, true);
assertEq(Object.getPrototypeOf(fn), AsyncFunction.prototype);


// "prototype" property is retrieved exactly once.
var trapLog = [], getLog = [];
var ProxiedConstructor = new Proxy(AsyncFunction, new Proxy({
    get(target, propertyKey, receiver) {
        getLog.push(propertyKey);
        return Reflect.get(target, propertyKey, receiver);
    }
}, {
    get(target, propertyKey, receiver) {
        trapLog.push(propertyKey);
        return Reflect.get(target, propertyKey, receiver);
    }
}));

fn = Reflect.construct(AsyncFunction, [], ProxiedConstructor);
assertEqArray(trapLog, ["get"]);
assertEqArray(getLog, ["prototype"]);
assertEq(fn instanceof AsyncFunction, true);
assertEq(Object.getPrototypeOf(fn), AsyncFunction.prototype);


if (typeof reportCompare === "function")
    reportCompare(0, 0);
