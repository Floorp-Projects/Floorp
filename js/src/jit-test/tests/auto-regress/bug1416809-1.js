// https://tc39.github.io/proposal-async-iteration

// Recursion between:
// 11.4.3.3 AsyncGeneratorResolve, step 8
// 11.4.3.5 AsyncGeneratorResumeNext, step 11.

var asyncIter = async function*(){ yield; }();
asyncIter.next();

for (var i = 0; i < 20000; i++) {
    asyncIter.next();
}
