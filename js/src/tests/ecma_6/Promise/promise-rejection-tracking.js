// |reftest| skip-if(!xulRuntime.shell) -- needs setPromiseRejectionTrackerCallback

if (!this.Promise) {
    this.reportCompare && reportCompare(true,true);
    quit(0);
}

const UNHANDLED = 0;
const HANDLED   = 1;

let rejections = new Map();
function rejectionTracker(promise, state) {
    rejections.set(promise, state);
}
setPromiseRejectionTrackerCallback(rejectionTracker);

// Unhandled rejections are tracked.
let reject;
let p = new Promise((res_, rej_) => (reject = rej_));
assertEq(rejections.has(p), false);
reject('reason');
assertEq(rejections.get(p), UNHANDLED);
// Later handling updates the tracking.
p.then(_=>_, _=>_);
assertEq(rejections.get(p), HANDLED);

rejections.clear();

// Handled rejections aren't tracked at all.
p = new Promise((res_, rej_) => (reject = rej_));
assertEq(rejections.has(p), false);
p.then(_=>_, _=>_);
reject('reason');
assertEq(rejections.has(p), false);

this.reportCompare && reportCompare(true,true);
