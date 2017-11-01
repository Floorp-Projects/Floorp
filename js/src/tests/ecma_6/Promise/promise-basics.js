// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

let results = [];

new Promise(res=>res('result'))
  .then(val=>{results.push('then ' + val); return 'first then rval';})
  .then(val=>results.push('chained then with val: ' + val));

new Promise((res, rej)=>rej('rejection'))
  .catch(val=>{results.push('catch ' + val); return results.length;})
  .then(val=>results.push('then after catch with val: ' + val),
        val=>{throw new Error("mustn't be called")});

new Promise((res, rej)=> {res('result');  rej('rejection'); })
  .catch(val=>{throw new Error("mustn't be called");})
  .then(val=>results.push('then after resolve+reject with val: ' + val),
        val=>{throw new Error("mustn't be called")});

new Promise((res, rej)=> { rej('rejection'); res('result'); })
  .catch(val=>{results.push('catch after reject+resolve with val: ' + val);})


drainJobQueue();

assertEq(results.length, 6);
assertEq(results[0], 'then result');
assertEq(results[1], 'catch rejection');
assertEq(results[2], 'catch after reject+resolve with val: rejection');
assertEq(results[3], 'chained then with val: first then rval');
assertEq(results[4], 'then after catch with val: 2');
assertEq(results[5], 'then after resolve+reject with val: result');

results = [];

Promise.resolve('resolution').then(res=>results.push(res),
                                   rej=>{ throw new Error("mustn't be called"); });

let thenCalled = false;
Promise.reject('rejection').then(_=>{thenCalled = true},
                                 rej=>results.push(rej));

drainJobQueue();

assertEq(thenCalled, false);
assertEq(results.length, 2);
assertEq(results[0], 'resolution');
assertEq(results[1], 'rejection');


function callback() {}

// Calling the executor function with content functions shouldn't assert:
Promise.resolve.call(function(exec) { exec(callback, callback); });
Promise.reject.call(function(exec) { exec(callback, callback); });
Promise.all.call(function(exec) { exec(callback, callback); });
Promise.race.call(function(exec) { exec(callback, callback); });

let resolveResult = undefined;
function resolveFun() {resolveResult = "resolveCalled";}
Promise.resolve.call(function(exec) { exec(resolveFun, callback); });
assertEq(resolveResult, "resolveCalled");

let rejectResult = undefined;
function rejectFun() {rejectResult = "rejectCalled";}
Promise.reject.call(function(exec) { exec(callback, rejectFun); });
assertEq(rejectResult, "rejectCalled");

// These should throw:
var wasCalled = false;
var hasThrown = false;
try {
    // Calling the executor function twice, providing a resolve callback both times.
    Promise.resolve.call(function(executor) {
        wasCalled = true;
        executor(callback, undefined);
        executor(callback, callback);
    });
} catch (e) {
    hasThrown = true;
}
assertEq(wasCalled, true);
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
