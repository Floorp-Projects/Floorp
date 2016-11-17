/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Test subclassing %Intl.NumberFormat% works correctly.
class MyNumberFormat extends Intl.NumberFormat {}

var obj = new MyNumberFormat();
assertEq(obj instanceof MyNumberFormat, true);
assertEq(obj instanceof Intl.NumberFormat, true);
assertEq(Object.getPrototypeOf(obj), MyNumberFormat.prototype);

obj = Reflect.construct(MyNumberFormat, []);
assertEq(obj instanceof MyNumberFormat, true);
assertEq(obj instanceof Intl.NumberFormat, true);
assertEq(Object.getPrototypeOf(obj), MyNumberFormat.prototype);

obj = Reflect.construct(MyNumberFormat, [], MyNumberFormat);
assertEq(obj instanceof MyNumberFormat, true);
assertEq(obj instanceof Intl.NumberFormat, true);
assertEq(Object.getPrototypeOf(obj), MyNumberFormat.prototype);

obj = Reflect.construct(MyNumberFormat, [], Intl.NumberFormat);
assertEq(obj instanceof MyNumberFormat, false);
assertEq(obj instanceof Intl.NumberFormat, true);
assertEq(Object.getPrototypeOf(obj), Intl.NumberFormat.prototype);


// Set a different constructor as NewTarget.
obj = Reflect.construct(MyNumberFormat, [], Array);
assertEq(obj instanceof MyNumberFormat, false);
assertEq(obj instanceof Intl.NumberFormat, false);
assertEq(obj instanceof Array, true);
assertEq(Object.getPrototypeOf(obj), Array.prototype);

obj = Reflect.construct(Intl.NumberFormat, [], Array);
assertEq(obj instanceof Intl.NumberFormat, false);
assertEq(obj instanceof Array, true);
assertEq(Object.getPrototypeOf(obj), Array.prototype);


// The prototype defaults to %NumberFormatPrototype% if null.
function NewTargetNullPrototype() {}
NewTargetNullPrototype.prototype = null;

obj = Reflect.construct(Intl.NumberFormat, [], NewTargetNullPrototype);
assertEq(obj instanceof Intl.NumberFormat, true);
assertEq(Object.getPrototypeOf(obj), Intl.NumberFormat.prototype);

obj = Reflect.construct(MyNumberFormat, [], NewTargetNullPrototype);
assertEq(obj instanceof MyNumberFormat, false);
assertEq(obj instanceof Intl.NumberFormat, true);
assertEq(Object.getPrototypeOf(obj), Intl.NumberFormat.prototype);


// "prototype" property is retrieved exactly once.
var trapLog = [], getLog = [];
var ProxiedConstructor = new Proxy(Intl.NumberFormat, new Proxy({
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

obj = Reflect.construct(Intl.NumberFormat, [], ProxiedConstructor);
assertEqArray(trapLog, ["get"]);
assertEqArray(getLog, ["prototype"]);
assertEq(obj instanceof Intl.NumberFormat, true);
assertEq(Object.getPrototypeOf(obj), Intl.NumberFormat.prototype);


if (typeof reportCompare === "function")
    reportCompare(0, 0);
