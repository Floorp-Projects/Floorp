// Based on testcases provided by André Bargull

let log = [];
let logger = new Proxy({}, {
    get(target, key) {
        log.push(key);
    }
});

Object.create(null, new Proxy({a: {value: 0}, b: {value: 1}}, logger));
assertEq(log.join(), "ownKeys,getOwnPropertyDescriptor,get,getOwnPropertyDescriptor,get");

log = [];
Object.defineProperties({}, new Proxy({a: {value: 0}, b: {value: 1}}, logger));
assertEq(log.join(), "ownKeys,getOwnPropertyDescriptor,get,getOwnPropertyDescriptor,get");

reportCompare(true, true);
