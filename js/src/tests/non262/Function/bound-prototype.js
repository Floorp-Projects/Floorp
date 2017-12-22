/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function Bound(fn) {
    return Function.prototype.bind.call(fn);
}

var fnDefaultPrototype = function(){};
assertEq(Object.getPrototypeOf(Bound(fnDefaultPrototype)), Function.prototype);

var fnGeneratorPrototype = function*(){};
assertEq(Object.getPrototypeOf(Bound(fnGeneratorPrototype)), function*(){}.constructor.prototype);

var fnCustomPrototype = Object.setPrototypeOf(function(){}, Array.prototype);
assertEq(Object.getPrototypeOf(Bound(fnCustomPrototype)), Array.prototype);

var fnNullPrototype = Object.setPrototypeOf(function(){}, null);
assertEq(Object.getPrototypeOf(Bound(fnNullPrototype)), null);


function LoggingProxy(target) {
    var log = [];
    var proxy = new Proxy(target, new Proxy({}, {
        get(target, propertyKey, receiver) {
            log.push(propertyKey);
        }
    }));
    return {proxy, log};
}

var {proxy, log} = LoggingProxy(function(){});
Bound(proxy);
assertEqArray(log, ["getPrototypeOf", "getOwnPropertyDescriptor", "get", "get"]);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
