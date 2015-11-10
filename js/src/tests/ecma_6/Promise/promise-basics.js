// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

if (!this.Promise) {
    this.reportCompare && reportCompare(0, 0, "ok");
    quit(0);
}

let results = [];

new Promise(res=>res('result'))
  .then(val=>{results.push('then ' + val); return 'first then rval';})
  .then(val=>results.push('chained then with val: ' + val));

new Promise((res, rej)=>rej('rejection'))
  .catch(val=>{results.push('catch ' + val); return results.length;})
  .then(val=>results.push('then after catch with val: ' + val),
        val=>{throw new Error("mustn't be called")});

drainJobQueue();

assertEq(results.length, 4);
assertEq(results[0], 'then result');
assertEq(results[1], 'catch rejection');
assertEq(results[2], 'chained then with val: first then rval');
assertEq(results[3], 'then after catch with val: 2');

function callback() {}

// Calling the executor function with content functions shouldn't assert:
Promise.resolve.call(function(exec) { exec(callback, callback); });
Promise.reject.call(function(exec) { exec(callback, callback); });
Promise.all.call(function(exec) { exec(callback, callback); });
Promise.race.call(function(exec) { exec(callback, callback); });

// These should throw:
var hasThrown = false;
try {
    // Calling the executor function twice, providing a resolve callback both times.
    Promise.resolve.call(function(executor) {
        executor(callback, undefined);
        executor(callback, callback);
    });
} catch (e) {
    hasThrown = true;
}
assertEq(hasThrown, true);

var hasThrown = false;
try {
    // Calling the executor function twice, providing a reject callback both times.
    Promise.resolve.call(function(executor) {
        executor(undefined, callback);
        executor(callback, callback);
    });
} catch (e) {
    hasThrown = true;
}
assertEq(hasThrown, true);

this.reportCompare && reportCompare(0, 0, "ok");
