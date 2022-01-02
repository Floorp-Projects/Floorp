/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function logProxy(object) {
    var log = [];
    var handler = {
        getOwnPropertyDescriptor(target, propertyKey) {
            log.push(propertyKey);
            return Object.getOwnPropertyDescriptor(target, propertyKey);
        }
    };
    var proxy = new Proxy(object, new Proxy(handler, {
        get(target, propertyKey, receiver) {
            if (!(propertyKey in target)) {
                throw new Error(`Unexpected call to trap: "${propertyKey}"`);
            }
            return target[propertyKey];
        }
    }));
    return {proxy, log};
}

var properties = ["string-property"];
if (typeof Symbol === 'function')
    properties.push(Symbol("symbol-property"));

for (var property of properties) {
    // Test 1: property is not present on object
    var {proxy, log} = logProxy({});
    var result = Object.prototype.propertyIsEnumerable.call(proxy, property);
    assertEq(result, false);
    assertDeepEq(log, [property]);

    // Test 2: property is present on object and enumerable
    var {proxy, log} = logProxy({[property]: 0});
    var result = Object.prototype.propertyIsEnumerable.call(proxy, property);
    assertEq(result, true);
    assertDeepEq(log, [property]);

    // Test 3: property is present on object, but not enumerable
    var {proxy, log} = logProxy(Object.defineProperty({[property]: 0}, property, {enumerable: false}));
    var result = Object.prototype.propertyIsEnumerable.call(proxy, property);
    assertEq(result, false);
    assertDeepEq(log, [property]);

    // Test 4: property is present on prototype object
    var {proxy, log} = logProxy(Object.create({[property]: 0}));
    var result = Object.prototype.propertyIsEnumerable.call(proxy, property);
    assertEq(result, false);
    assertDeepEq(log, [property]);

    // Test 5: property is present on prototype object, prototype is proxy object
    var {proxy, log} = logProxy({[property]: 0});
    var result = Object.prototype.propertyIsEnumerable.call(Object.create(proxy), property);
    assertEq(result, false);
    assertDeepEq(log, []);
}

reportCompare(0, 0);
