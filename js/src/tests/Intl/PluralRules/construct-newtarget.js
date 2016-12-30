// |reftest| skip-if(!this.hasOwnProperty("Intl")||!this.hasOwnProperty("addIntlExtras"))

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

addIntlExtras(Intl);

// Test subclassing %Intl.PluralRules% works correctly.
class MyPluralRules extends Intl.PluralRules {}

var obj = new MyPluralRules();
assertEq(obj instanceof MyPluralRules, true);
assertEq(obj instanceof Intl.PluralRules, true);
assertEq(Object.getPrototypeOf(obj), MyPluralRules.prototype);

obj = Reflect.construct(MyPluralRules, []);
assertEq(obj instanceof MyPluralRules, true);
assertEq(obj instanceof Intl.PluralRules, true);
assertEq(Object.getPrototypeOf(obj), MyPluralRules.prototype);

obj = Reflect.construct(MyPluralRules, [], MyPluralRules);
assertEq(obj instanceof MyPluralRules, true);
assertEq(obj instanceof Intl.PluralRules, true);
assertEq(Object.getPrototypeOf(obj), MyPluralRules.prototype);

obj = Reflect.construct(MyPluralRules, [], Intl.PluralRules);
assertEq(obj instanceof MyPluralRules, false);
assertEq(obj instanceof Intl.PluralRules, true);
assertEq(Object.getPrototypeOf(obj), Intl.PluralRules.prototype);


// Set a different constructor as NewTarget.
obj = Reflect.construct(MyPluralRules, [], Array);
assertEq(obj instanceof MyPluralRules, false);
assertEq(obj instanceof Intl.PluralRules, false);
assertEq(obj instanceof Array, true);
assertEq(Object.getPrototypeOf(obj), Array.prototype);

obj = Reflect.construct(Intl.PluralRules, [], Array);
assertEq(obj instanceof Intl.PluralRules, false);
assertEq(obj instanceof Array, true);
assertEq(Object.getPrototypeOf(obj), Array.prototype);


// The prototype defaults to %PluralRulesPrototype% if null.
function NewTargetNullPrototype() {}
NewTargetNullPrototype.prototype = null;

obj = Reflect.construct(Intl.PluralRules, [], NewTargetNullPrototype);
assertEq(obj instanceof Intl.PluralRules, true);
assertEq(Object.getPrototypeOf(obj), Intl.PluralRules.prototype);

obj = Reflect.construct(MyPluralRules, [], NewTargetNullPrototype);
assertEq(obj instanceof MyPluralRules, false);
assertEq(obj instanceof Intl.PluralRules, true);
assertEq(Object.getPrototypeOf(obj), Intl.PluralRules.prototype);


// "prototype" property is retrieved exactly once.
var trapLog = [], getLog = [];
var ProxiedConstructor = new Proxy(Intl.PluralRules, new Proxy({
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

obj = Reflect.construct(Intl.PluralRules, [], ProxiedConstructor);
assertEqArray(trapLog, ["get"]);
assertEqArray(getLog, ["prototype"]);
assertEq(obj instanceof Intl.PluralRules, true);
assertEq(Object.getPrototypeOf(obj), Intl.PluralRules.prototype);


if (typeof reportCompare === "function")
    reportCompare(0, 0);
