/*---
 author: Tooru Fujisawa [:arai] <arai_a@mac.com>
 esid: pending
 description: execution order for yield* with async iterator and return()
 info: >
    YieldExpression: yield * AssignmentExpression

    ...
    6. Repeat
      ...
      c. Else,
        i. Assert: received.[[Type]] is return.
        ii. Let return be ? GetMethod(iterator, "return").
        iii. If return is undefined, return Completion(received).
        iv. Let innerReturnResult be ? Call(return, iterator,
            « received.[[Value]] »).
        v. If generatorKind is async, then set innerReturnResult to
           ? Await(innerReturnResult).
        ...
        vii. Let done be ? IteratorComplete(innerReturnResult).
        viii. If done is true, then
             1. Let value be ? IteratorValue(innerReturnResult).
             2. Return Completion{[[Type]]: return, [[Value]]: value,
                [[Target]]: empty}.
        ix. Let received be GeneratorYield(innerResult).

    GeneratorYield ( iterNextObj )

    ...
    10. If generatorKind is async,
      a. Let value be IteratorValue(iterNextObj).
      b. Let done be IteratorComplete(iterNextObj).
      c. Return ! AsyncGeneratorResolve(generator, value, done).
    ...

 flags: [async]
---*/

var log = [];
var iter = {
  [Symbol.asyncIterator]() {
    var returnCount = 0;
    return {
      name: 'asyncIterator',
      get next() {
        log.push({ name: "get next" });
        return function() {
          return {
            value: "next-value-1",
            done: false
          };
        };
      },
      get return() {
        log.push({
          name: "get return",
          thisValue: this
        });
        return function() {
          log.push({
            name: "call return",
            thisValue: this,
            args: [...arguments]
          });

          returnCount++;
          if (returnCount == 1) {
            return {
              name: "return-promise-1",
              get then() {
                log.push({
                  name: "get return then (1)",
                  thisValue: this
                });
                return function(resolve) {
                  log.push({
                    name: "call return then (1)",
                    thisValue: this,
                    args: [...arguments]
                  });

                  resolve({
                    name: "return-result-1",
                    get value() {
                      log.push({
                        name: "get return value (1)",
                        thisValue: this
                      });
                      return "return-value-1";
                    },
                    get done() {
                      log.push({
                        name: "get return done (1)",
                        thisValue: this
                      });
                      return false;
                    }
                  });
                };
              }
            };
          }

          return {
            name: "return-promise-2",
            get then() {
              log.push({
                name: "get return then (2)",
                thisValue: this
              });
              return function(resolve) {
                log.push({
                  name: "call return then (2)",
                  thisValue: this,
                  args: [...arguments]
                });

                resolve({
                  name: "return-result-2",
                  get value() {
                    log.push({
                      name: "get return value (2)",
                      thisValue: this
                    });
                    return "return-value-2";
                  },
                  get done() {
                    log.push({
                      name: "get return done (2)",
                      thisValue: this
                    });
                    return true;
                  }
                });
              };
            }
          };
        };
      }
    };
  }
};
var asyncIterator = (async function*() {
  log.push({ name: "before yield*" });
  yield* iter;
})();

assert.sameValue(log.length, 0, "log.length");

asyncIterator.next().then(v => {
  assert.sameValue(log[0].name, "before yield*");

  assert.sameValue(log[1].name, "get next");

  assert.sameValue(v.value, "next-value-1");
  assert.sameValue(v.done, false);

  assert.sameValue(log.length, 2, "log.length");

  asyncIterator.return("return-arg-1").then(v => {
    assert.sameValue(log[2].name, "get return");
    assert.sameValue(log[2].thisValue.name, "asyncIterator", "get return thisValue");

    assert.sameValue(log[3].name, "call return");
    assert.sameValue(log[3].thisValue.name, "asyncIterator", "return thisValue");
    assert.sameValue(log[3].args.length, 1, "return args.length");
    assert.sameValue(log[3].args[0], "return-arg-1", "return args[0]");

    assert.sameValue(log[4].name, "get return then (1)");
    assert.sameValue(log[4].thisValue.name, "return-promise-1", "get return then thisValue");

    assert.sameValue(log[5].name, "call return then (1)");
    assert.sameValue(log[5].thisValue.name, "return-promise-1", "return then thisValue");
    assert.sameValue(log[5].args.length, 2, "return then args.length");
    assert.sameValue(typeof log[5].args[0], "function", "return then args[0]");
    assert.sameValue(typeof log[5].args[1], "function", "return then args[1]");

    assert.sameValue(log[6].name, "get return done (1)");
    assert.sameValue(log[6].thisValue.name, "return-result-1", "get return done thisValue");

    assert.sameValue(log[7].name, "get return value (1)");
    assert.sameValue(log[7].thisValue.name, "return-result-1", "get return value thisValue");

    assert.sameValue(log[8].name, "get return done (1)");
    assert.sameValue(log[8].thisValue.name, "return-result-1", "get return done thisValue");

    assert.sameValue(v.value, "return-value-1");
    assert.sameValue(v.done, false);

    assert.sameValue(log.length, 9, "log.length");

    asyncIterator.return("return-arg-2").then(v => {
      assert.sameValue(log[9].name, "get return");
      assert.sameValue(log[9].thisValue.name, "asyncIterator", "get return thisValue");

      assert.sameValue(log[10].name, "call return");
      assert.sameValue(log[10].thisValue.name, "asyncIterator", "return thisValue");
      assert.sameValue(log[10].args.length, 1, "return args.length");
      assert.sameValue(log[10].args[0], "return-arg-2", "return args[0]");

      assert.sameValue(log[11].name, "get return then (2)");
      assert.sameValue(log[11].thisValue.name, "return-promise-2", "get return then thisValue");

      assert.sameValue(log[12].name, "call return then (2)");
      assert.sameValue(log[12].thisValue.name, "return-promise-2", "return then thisValue");
      assert.sameValue(log[12].args.length, 2, "return then args.length");
      assert.sameValue(typeof log[12].args[0], "function", "return then args[0]");
      assert.sameValue(typeof log[12].args[1], "function", "return then args[1]");

      assert.sameValue(log[13].name, "get return done (2)");
      assert.sameValue(log[13].thisValue.name, "return-result-2", "get return done thisValue");

      assert.sameValue(log[14].name, "get return value (2)");
      assert.sameValue(log[14].thisValue.name, "return-result-2", "get return value thisValue");

      assert.sameValue(v.value, "return-value-2");
      assert.sameValue(v.done, true);

      assert.sameValue(log.length, 15, "log.length");
    }).then($DONE, $DONE);
  }).catch($DONE);
}).catch($DONE);
