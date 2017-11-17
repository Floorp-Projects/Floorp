// https://tc39.github.io/proposal-async-iteration

// Recursion between:
// 11.4.3.4 AsyncGeneratorReject, step 7.
// 11.4.3.5 AsyncGeneratorResumeNext, step 10.b.ii.2.

var asyncIter = async function*(){ yield; }();
asyncIter.next();

for (var i = 0; i < 20000; i++) {
    asyncIter.throw();
}
