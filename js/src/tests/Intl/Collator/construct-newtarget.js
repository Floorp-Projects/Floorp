/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Test subclassing %Intl.Collator% works correctly.
class MyCollator extends Intl.Collator {}

var obj = new MyCollator();
assertEq(obj instanceof MyCollator, true);
assertEq(obj instanceof Intl.Collator, true);
assertEq(Object.getPrototypeOf(obj), MyCollator.prototype);

obj = Reflect.construct(MyCollator, []);
assertEq(obj instanceof MyCollator, true);
assertEq(obj instanceof Intl.Collator, true);
assertEq(Object.getPrototypeOf(obj), MyCollator.prototype);

obj = Reflect.construct(MyCollator, [], MyCollator);
assertEq(obj instanceof MyCollator, true);
assertEq(obj instanceof Intl.Collator, true);
assertEq(Object.getPrototypeOf(obj), MyCollator.prototype);

obj = Reflect.construct(MyCollator, [], Intl.Collator);
assertEq(obj instanceof MyCollator, false);
assertEq(obj instanceof Intl.Collator, true);
assertEq(Object.getPrototypeOf(obj), Intl.Collator.prototype);


// Set a different constructor as NewTarget.
obj = Reflect.construct(MyCollator, [], Array);
assertEq(obj instanceof MyCollator, false);
assertEq(obj instanceof Intl.Collator, false);
assertEq(obj instanceof Array, true);
assertEq(Object.getPrototypeOf(obj), Array.prototype);

obj = Reflect.construct(Intl.Collator, [], Array);
assertEq(obj instanceof Intl.Collator, false);
assertEq(obj instanceof Array, true);
assertEq(Object.getPrototypeOf(obj), Array.prototype);


// The prototype defaults to %CollatorPrototype% if null.
function NewTargetNullPrototype() {}
NewTargetNullPrototype.prototype = null;

obj = Reflect.construct(Intl.Collator, [], NewTargetNullPrototype);
assertEq(obj instanceof Intl.Collator, true);
assertEq(Object.getPrototypeOf(obj), Intl.Collator.prototype);

obj = Reflect.construct(MyCollator, [], NewTargetNullPrototype);
assertEq(obj instanceof MyCollator, false);
assertEq(obj instanceof Intl.Collator, true);
assertEq(Object.getPrototypeOf(obj), Intl.Collator.prototype);


// "prototype" property is retrieved exactly once.
var trapLog = [], getLog = [];
var ProxiedConstructor = new Proxy(Intl.Collator, new Proxy({
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

obj = Reflect.construct(Intl.Collator, [], ProxiedConstructor);
assertEqArray(trapLog, ["get"]);
assertEqArray(getLog, ["prototype"]);
assertEq(obj instanceof Intl.Collator, true);
assertEq(Object.getPrototypeOf(obj), Intl.Collator.prototype);


if (typeof reportCompare === "function")
    reportCompare(0, 0);
