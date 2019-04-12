// |jit-test| --enable-experimental-await-fix
// <https://github.com/tc39/ecma262/pull/1470> changes a detail of
// error-handling in %AsyncFromSyncIteratorPrototype% methods. This test is
// based on a comment in the thread where the issue was first reported,
// <https://github.com/tc39/ecma262/issues/1461#issuecomment-468602852>

let log = [];

{
  async function f() {
    var p = Promise.resolve(0);
    Object.defineProperty(p, "constructor", {get() { throw "hi" }});
    for await (var x of [p]);
  }
  Promise.resolve(0)
         .then(() => log.push("tick 1"))
         .then(() => log.push("tick 2"))
         .then(() => log.push("tick 3"));
  f().catch(exc => log.push(exc));
}

drainJobQueue();
assertEq(log.join(), "tick 1,tick 2,hi,tick 3");
