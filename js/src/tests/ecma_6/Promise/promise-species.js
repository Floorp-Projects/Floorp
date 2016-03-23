if (!this.Promise) {
    reportCompare(true,true);
    quit(0);
}

assertEq(Promise[Symbol.species], Promise);
let prop = Object.getOwnPropertyDescriptor(Promise, Symbol.species);
assertEq('get' in prop, true);
assertEq(typeof prop.get, 'function');
assertEq('set' in prop, true);
assertEq(prop.set, undefined);

reportCompare(0, 0, "ok");
