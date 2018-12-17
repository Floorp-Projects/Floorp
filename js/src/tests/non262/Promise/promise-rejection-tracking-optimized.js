// |reftest| skip-if(!xulRuntime.shell) -- needs setPromiseRejectionTrackerCallback

const UNHANDLED = 0;
const HANDLED   = 1;

let rejections = new Map();
function rejectionTracker(promise, state) {
  rejections.set(promise, state);
}
setPromiseRejectionTrackerCallback(rejectionTracker);

// If the return value of then is not used, the promise object is optimized
// away, but if a rejection happens, the rejection should be notified.
Promise.resolve().then(() => { throw 1; });
drainJobQueue();

assertEq(rejections.size, 1);

let [[promise, state]] = rejections;
assertEq(state, UNHANDLED);

let exc;
promise.catch(x => { exc = x; });
drainJobQueue();

// we handled it after all
assertEq(rejections.get(promise), HANDLED);

// the right exception was reported
assertEq(exc, 1);

if (this.reportCompare) {
  reportCompare(true,true);
}
