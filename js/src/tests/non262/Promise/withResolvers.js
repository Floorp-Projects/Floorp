// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

{
  let {resolve, promise} = Promise.withResolvers();

  let result = undefined;
  promise.then((v) => result = v);
  resolve(5);

  drainJobQueue();
  assertEq(result, 5);
}

{
  let {reject, promise} = Promise.withResolvers();

  let result = undefined;
  promise.catch((v) => result = v);
  reject("abc");

  drainJobQueue();
  assertEq(result, "abc");
}

reportCompare(true,true);
