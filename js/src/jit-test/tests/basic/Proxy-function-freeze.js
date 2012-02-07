// Once a function proxy has been frozen, its handler's traps are no longer called,
// but its call traps are.

var callTrapCalls;
function handleCall() {
    print('call');
    callTrapCalls++;
    return 'favor';
}
function handleConstruct() {
    print('construct');
    callTrapCalls++;
    return 'compliment';
}

var descriptorForX = { configurable: true, enumerable: true, writable: true, value: 42 };
var trapCalls;
var handler = {
    getOwnPropertyNames: function () {
        print('getOwnPropertyNames');
        trapCalls++;
        return ['x'];
    },
    getPropertyNames: function() {
        print('getPropertyNames');
        trapCalls++;
        return ['x'];
    },
    getOwnPropertyDescriptor: function(name) {
        print('getOwnPropertyDescriptor');
        trapCalls++;
        assertEq(name, 'x');
        return descriptorForX;
    },
    getPropertyDescriptor: function(name) {
        print('getPropertyDescriptor');
        trapCalls++;
        assertEq(name, 'x');
        return descriptorForX;
    },
    defineProperty: function(name, propertyDescriptor) {
        print('defineProperty');
        trapCalls++;
    },
    delete: function(name) {
        print('delete');
        trapCalls++;
        return false;
    },
    fix: function() {
        print('fix');
        trapCalls++;
        return { x: descriptorForX };
    }
};

var fp = Proxy.createFunction(handler, handleCall, handleConstruct);

trapCalls = callTrapCalls = 0;
assertEq(Object.getOwnPropertyNames(fp)[0], 'x');
assertEq(trapCalls > 0, true);
assertEq(callTrapCalls, 0);

trapCalls = callTrapCalls = 0;
assertEq(Object.getOwnPropertyDescriptor(fp, 'x').value, 42);
assertEq(trapCalls > 0, true);
assertEq(callTrapCalls, 0);

trapCalls = callTrapCalls = 0;
assertEq(delete fp.x, false);
assertEq(trapCalls > 0, true);
assertEq(callTrapCalls, 0);

trapCalls = callTrapCalls = 0;
assertEq(fp(), 'favor');
assertEq(trapCalls, 0);
assertEq(callTrapCalls, 1);

trapCalls = callTrapCalls = 0;
assertEq(new fp, 'compliment');
assertEq(trapCalls, 0);
assertEq(callTrapCalls, 1);

trapCalls = callTrapCalls = 0;
Object.freeze(fp);
assertEq(trapCalls > 0, true);
assertEq(callTrapCalls, 0);

// Once the proxy has been frozen, its traps should never be invoked any
// more.
trapCalls = callTrapCalls = 0;
assertEq(Object.getOwnPropertyNames(fp)[0], 'x');
assertEq(trapCalls, 0);

trapCalls = callTrapCalls = 0;
assertEq(Object.getOwnPropertyDescriptor(fp, 'x').value, 42);
assertEq(trapCalls, 0);

trapCalls = callTrapCalls = 0;
assertEq(delete fp.x, false);
assertEq(trapCalls, 0);

trapCalls = callTrapCalls = 0;
assertEq(fp(), 'favor');
assertEq(trapCalls, 0);
assertEq(callTrapCalls, 1);

trapCalls = callTrapCalls = 0;
assertEq(new fp, 'compliment');
assertEq(trapCalls, 0);
assertEq(callTrapCalls, 1);

trapCalls = callTrapCalls = 0;
Object.freeze(fp);
assertEq(trapCalls, 0);
