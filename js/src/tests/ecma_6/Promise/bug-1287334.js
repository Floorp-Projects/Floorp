var promise = Promise.resolve(1);
var FakeCtor = function(exec){ exec(function(){}, function(){}); };
Object.defineProperty(Promise, Symbol.species, {value: FakeCtor});
// This just shouldn't crash. It does without bug 1287334 fixed.
promise.then(function(){});

this.reportCompare && reportCompare(true, true);
