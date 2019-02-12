// |jit-test| error: "async tests completed successfully"
// Test that the shell's job queue doesn't skip calls to JS::JobQueueMayNotBeEmpty.

// For expressions like `await 1`, or `await P` for some already-resolved
// promise P, there's no need to suspend the async call to determine the
// expression's value. Suspension and resumption are expensive, so it would be
// nice if we could avoid them.
//
// But of course, even when the value is known, the act of suspension itself is
// visible: an async call's first suspension returns to its (synchronous)
// caller; subsequent suspensions let other jobs run. So in general, we can't
// short-circuit such `await` expressions.
//
// However, if an async call has been resumed from the job queue (that is, this
// isn't the initial execution, with a synchronous caller expecting a promise of
// the call's final return value), and there are no other jobs following that,
// then the `await`'s reaction job would run immediately following this job ---
// which *is* indistinguishable from skipping the suspension altogether.
//
// A JS::JobQueue implementation may call JS::JobQueueIsEmpty to indicate to the
// engine that the currently running job is the last job in the queue, so this
// optimization may be considered (there are further conditions that must be met
// as well). If the JobQueue calls JobQueueIsEmpty, then it must also call
// JS::JobQueueMayNotBeEmpty when jobs are enqueued, to indicate when the
// opportunity has passed.

var log = '';
async function f(label, k) {
  log += label + '1';
  await 1;
  log += label + '2';
  await 1;
  log += label + '3';

  return k();
}

// Call `f` with `label` and `k`. If `skippable` is true, exercise the path that
// skips the suspension and resumption; otherwise exercise the
// non-short-circuited path.
function test(skippable, label, k) {
  var resolve;
  (new Promise(r => { resolve = r; }))
    .then(v => { log += v + 't'; });
  assertEq(log, '');
  f(label, k);
  // job queue now: f(label)'s first await's continuation
  assertEq(log, label + '1');

  if (!skippable) {
    resolve('p');
    assertEq(log, label + '1');
    // job queue now: f(label)'s first await's continuation, explicit promise's reaction
  }

  // Resuming f(label) will reach the second await, which should skip suspension
  // or not, depending on whether we resolved that promise.
}

// SpiderMonkey's internal 'queue is empty' flag is initially false, even though
// the queue is initially empty, because we don't yet know whether the embedding
// is going to participate in the optimization by calling
// JS::JobQueueMayNotBeEmpty and JS::JobQueueIsEmpty. But since the shell uses
// SpiderMonkey's internal job queue implementation, this call to
// `drainJobQueue` calls `JS::JobQueueIsEmpty`, and we are ready to play.
Promise.resolve(42).then(v => assertEq(v, 42));
drainJobQueue();

log = '';
test(true, 'b', continuation1);

function continuation1() {
  assertEq(log, 'b1b2b3');

  log = '';
  test(false, 'c', continuation2);
}

function continuation2() {
  assertEq(log, 'c1c2ptc3');
  throw "async tests completed successfully"; // proof that we actually finished
}
