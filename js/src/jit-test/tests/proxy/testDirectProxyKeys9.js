// Cull non-existent names returned by the trap.
var nullProtoAB = Object.create(null, {
    a: {
        enumerable: true,
        configurable: true
    },
    b: {
        enumerable: false,
        configurable: true
    }
});
var protoABWithCD = Object.create(nullProtoAB, {
    c: {
        enumerable: true,
        configurable: true
    },
    d: {
        enumerable: false,
        configurable: true
    }
});

var returnedNames;
var proxyTargetingCDWithKeys = new Proxy(protoABWithCD, {
    ownKeys: function (target) {
        return returnedNames;
    }
});

returnedNames = [ 'e' ];
var names = Object.keys(proxyTargetingCDWithKeys);
assertEq(names.length, 0);

returnedNames = [ 'c' ];
names = Object.keys(proxyTargetingCDWithKeys);
assertEq(names.length, 1);
assertEq(names[0], 'c');
