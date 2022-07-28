// Test module fields related to asynchronous evaluation.

// Hardcoded values of ModuleStatus. Keep these in sync if the code changes.
const StatusUnlinked = 0;
const StatusLinked = 2;
const StatusEvaluating = 3;
const StatusEvaluatingAsync = 4;
const StatusEvaluated = 5;

{
  let m = parseModule('');
  assertEq(m.status, StatusUnlinked);

  moduleLink(m);
  assertEq(m.isAsyncEvaluating, false);
  assertEq(m.status, StatusLinked);

  moduleEvaluate(m);
  assertEq(m.isAsyncEvaluating, false);
  assertEq(m.status, StatusEvaluated);
}

{
  let m = parseModule('await 1;');

  moduleLink(m);
  assertEq(m.isAsyncEvaluating, false);

  moduleEvaluate(m);
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, StatusEvaluatingAsync);
  assertEq(m.asyncEvaluatingPostOrder, 1);

  drainJobQueue();
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, StatusEvaluated);
  assertEq(m.asyncEvaluatingPostOrder, 0);
}

{
  let m = parseModule('await 1; throw 2;');

  moduleLink(m);
  moduleEvaluate(m).catch(() => 0);
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, StatusEvaluatingAsync);
  assertEq(m.asyncEvaluatingPostOrder, 1);

  drainJobQueue();
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, StatusEvaluated);
  assertEq(m.evaluationError, 2);
  assertEq(m.asyncEvaluatingPostOrder, 0);
}

{
  let m = parseModule('throw 1; await 2;');
  moduleLink(m);
  moduleEvaluate(m).catch(() => 0);
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, StatusEvaluatingAsync);
  assertEq(m.asyncEvaluatingPostOrder, 1);

  drainJobQueue();
  assertEq(m.isAsyncEvaluating, true);
  assertEq(m.status, StatusEvaluated);
  assertEq(m.evaluationError, 1);
  assertEq(m.asyncEvaluatingPostOrder, 0);
}

{
  clearModules();
  let a = registerModule('a', parseModule(''));
  let b = registerModule('b', parseModule('import {} from "a"; await 1;'));

  moduleLink(b);
  moduleEvaluate(b);
  assertEq(a.isAsyncEvaluating, false);
  assertEq(a.status, StatusEvaluated);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluatingAsync);
  assertEq(b.asyncEvaluatingPostOrder, 1);

  drainJobQueue();
  assertEq(a.isAsyncEvaluating, false);
  assertEq(a.status, StatusEvaluated);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluated);
  assertEq(b.asyncEvaluatingPostOrder, 0);
}

{
  clearModules();
  let a = registerModule('a', parseModule('await 1;'));
  let b = registerModule('b', parseModule('import {} from "a";'));

  moduleLink(b);
  moduleEvaluate(b);
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, StatusEvaluatingAsync);
  assertEq(a.asyncEvaluatingPostOrder, 1);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluatingAsync);
  assertEq(b.asyncEvaluatingPostOrder, 2);

  drainJobQueue();
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, StatusEvaluated);
  assertEq(a.asyncEvaluatingPostOrder, 0);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluated);
  assertEq(b.asyncEvaluatingPostOrder, 0);
}

{
  clearModules();
  let resolve;
  var promise = new Promise(r => { resolve = r; });
  let a = registerModule('a', parseModule('await promise;'));
  let b = registerModule('b', parseModule('await 2;'));
  let c = registerModule('c', parseModule('import {} from "a"; import {} from "b";'));

  moduleLink(c);
  moduleEvaluate(c);
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, StatusEvaluatingAsync);
  assertEq(a.asyncEvaluatingPostOrder, 1);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluatingAsync);
  assertEq(b.asyncEvaluatingPostOrder, 2);
  assertEq(c.isAsyncEvaluating, true);
  assertEq(c.status, StatusEvaluatingAsync);
  assertEq(c.asyncEvaluatingPostOrder, 3);

  resolve(1);
  drainJobQueue();
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, StatusEvaluated);
  assertEq(a.asyncEvaluatingPostOrder, 0);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluated);
  assertEq(b.asyncEvaluatingPostOrder, 0);
  assertEq(c.isAsyncEvaluating, true);
  assertEq(c.status, StatusEvaluated);
  assertEq(c.asyncEvaluatingPostOrder, 0);
}

{
  clearModules();
  let a = registerModule('a', parseModule('throw 1;'));
  let b = registerModule('b', parseModule('import {} from "a"; await 2;'));

  moduleLink(b);
  moduleEvaluate(b).catch(() => 0);
  assertEq(a.status, StatusEvaluated);
  assertEq(a.isAsyncEvaluating, false);
  assertEq(a.evaluationError, 1);
  assertEq(b.status, StatusEvaluated);
  assertEq(b.isAsyncEvaluating, false);
  assertEq(b.evaluationError, 1);
}

{
  clearModules();
  let a = registerModule('a', parseModule('throw 1; await 2;'));
  let b = registerModule('b', parseModule('import {} from "a";'));

  moduleLink(b);
  moduleEvaluate(b).catch(() => 0);
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, StatusEvaluatingAsync);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluatingAsync);

  drainJobQueue();
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, StatusEvaluated);
  assertEq(a.evaluationError, 1);
  assertEq(a.asyncEvaluatingPostOrder, 0);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluated);
  assertEq(b.evaluationError, 1);
  assertEq(b.asyncEvaluatingPostOrder, 0);
}

{
  clearModules();
  let a = registerModule('a', parseModule('await 1; throw 2;'));
  let b = registerModule('b', parseModule('import {} from "a";'));

  moduleLink(b);
  moduleEvaluate(b).catch(() => 0);
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, StatusEvaluatingAsync);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluatingAsync);

  drainJobQueue();
  assertEq(a.isAsyncEvaluating, true);
  assertEq(a.status, StatusEvaluated);
  assertEq(a.evaluationError, 2);
  assertEq(a.asyncEvaluatingPostOrder, 0);
  assertEq(b.isAsyncEvaluating, true);
  assertEq(b.status, StatusEvaluated);
  assertEq(b.evaluationError, 2);
  assertEq(b.asyncEvaluatingPostOrder, 0);
}
