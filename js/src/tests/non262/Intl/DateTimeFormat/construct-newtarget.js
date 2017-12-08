// |reftest| skip-if(!this.hasOwnProperty("Intl"))

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Test subclassing %Intl.DateTimeFormat% works correctly.
class MyDateTimeFormat extends Intl.DateTimeFormat {}

var obj = new MyDateTimeFormat();
assertEq(obj instanceof MyDateTimeFormat, true);
assertEq(obj instanceof Intl.DateTimeFormat, true);
assertEq(Object.getPrototypeOf(obj), MyDateTimeFormat.prototype);

obj = Reflect.construct(MyDateTimeFormat, []);
assertEq(obj instanceof MyDateTimeFormat, true);
assertEq(obj instanceof Intl.DateTimeFormat, true);
assertEq(Object.getPrototypeOf(obj), MyDateTimeFormat.prototype);

obj = Reflect.construct(MyDateTimeFormat, [], MyDateTimeFormat);
assertEq(obj instanceof MyDateTimeFormat, true);
assertEq(obj instanceof Intl.DateTimeFormat, true);
assertEq(Object.getPrototypeOf(obj), MyDateTimeFormat.prototype);

obj = Reflect.construct(MyDateTimeFormat, [], Intl.DateTimeFormat);
assertEq(obj instanceof MyDateTimeFormat, false);
assertEq(obj instanceof Intl.DateTimeFormat, true);
assertEq(Object.getPrototypeOf(obj), Intl.DateTimeFormat.prototype);


// Set a different constructor as NewTarget.
obj = Reflect.construct(MyDateTimeFormat, [], Array);
assertEq(obj instanceof MyDateTimeFormat, false);
assertEq(obj instanceof Intl.DateTimeFormat, false);
assertEq(obj instanceof Array, true);
assertEq(Object.getPrototypeOf(obj), Array.prototype);

obj = Reflect.construct(Intl.DateTimeFormat, [], Array);
assertEq(obj instanceof Intl.DateTimeFormat, false);
assertEq(obj instanceof Array, true);
assertEq(Object.getPrototypeOf(obj), Array.prototype);


// The prototype defaults to %DateTimeFormatPrototype% if null.
function NewTargetNullPrototype() {}
NewTargetNullPrototype.prototype = null;

obj = Reflect.construct(Intl.DateTimeFormat, [], NewTargetNullPrototype);
assertEq(obj instanceof Intl.DateTimeFormat, true);
assertEq(Object.getPrototypeOf(obj), Intl.DateTimeFormat.prototype);

obj = Reflect.construct(MyDateTimeFormat, [], NewTargetNullPrototype);
assertEq(obj instanceof MyDateTimeFormat, false);
assertEq(obj instanceof Intl.DateTimeFormat, true);
assertEq(Object.getPrototypeOf(obj), Intl.DateTimeFormat.prototype);


// "prototype" property is retrieved exactly once.
var trapLog = [], getLog = [];
var ProxiedConstructor = new Proxy(Intl.DateTimeFormat, new Proxy({
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

obj = Reflect.construct(Intl.DateTimeFormat, [], ProxiedConstructor);
assertEqArray(trapLog, ["get"]);
assertEqArray(getLog, ["prototype"]);
assertEq(obj instanceof Intl.DateTimeFormat, true);
assertEq(Object.getPrototypeOf(obj), Intl.DateTimeFormat.prototype);


if (typeof reportCompare === "function")
    reportCompare(0, 0);
