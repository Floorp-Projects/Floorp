// |reftest| skip-if(!xulRuntime.shell) -- needs getSelfHostedValue and drainJobQueue

function onResolved(val) {
    result = 'resolved with ' + val;
}

function onRejected(val) {
    result = 'rejected with ' + val;
}

// Replacing `Promise#then` shouldn't affect getWaitForAllPromise.
let originalThen = Promise.prototype.then;
Promise.prototype.then = 1;

// Replacing Promise[@@species] shouldn't affect getWaitForAllPromise.
Promise[Symbol.species] = function(){};

// Replacing `Promise` shouldn't affect getWaitForAllPromise.
let PromiseCtor = Promise;
Promise = {};

// Replacing Array[@@iterator] shouldn't affect getWaitForAllPromise.
Array.prototype[Symbol.iterator] = function(){};

let resolveFunctions = [];
let rejectFunctions = [];
let promises = [];
for (let i = 0; i < 3; i++) {
    let p = new PromiseCtor(function(res_, rej_) {
        resolveFunctions.push(res_);
        rejectFunctions.push(rej_);
    });
    promises.push(p);
}

let allPromise = getWaitForAllPromise(promises);
let then = originalThen.call(allPromise, onResolved, onRejected);

resolveFunctions.forEach((fun, i)=>fun(i));
drainJobQueue();

assertEq(result, 'resolved with 0,1,2');

// Empty lists result in a promise resolved with an empty array.
result = undefined;
originalThen.call(getWaitForAllPromise([]), v=>(result = v));
drainJobQueue();
assertEq(result instanceof Array, true);
assertEq(result.length, 0);

//Empty lists result in a promise resolved with an empty array.
result = undefined;
originalThen.call(getWaitForAllPromise([]), v=>(result = v));

drainJobQueue();

assertEq(result instanceof Array, true);
assertEq(result.length, 0);

this.reportCompare && reportCompare(true,true);
