// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

if (!this.Promise) {
    this.reportCompare && reportCompare(true,true);
    quit(0);
}

let resolve;
let promise = new Promise(function(x) { resolve = x; });
resolve(promise)

let results = [];
promise.then(res => assertEq(true, false, "not reached")).catch(res => {
    assertEq(res instanceof TypeError, true);
    results.push("rejected");
});

drainJobQueue()

assertEq(results.length, 1);
assertEq(results[0], "rejected");

this.reportCompare && reportCompare(0, 0, "ok");
