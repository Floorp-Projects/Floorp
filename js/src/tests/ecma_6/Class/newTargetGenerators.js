function *generatorNewTarget(expected) {
    assertEq(new.target, expected);
    assertEq(eval('new.target'), expected);
    assertEq((() => new.target)(), expected);
    yield (() => new.target);
}

const ITERATIONS = 25;

for (let i = 0; i < ITERATIONS; i++)
    assertEq(generatorNewTarget(undefined).next().value(), undefined);

for (let i = 0; i < ITERATIONS; i++)
    assertEq(new generatorNewTarget(generatorNewTarget).next().value(),
             generatorNewTarget);

// also check to make sure it's useful in yield inside generators.
// Plus, this code is so ugly, how could it not be a test? ;)
// Thanks to anba for supplying this ludicrous expression.
assertDeepEq([...new function*(i) { yield i; if(i > 0) yield* new new.target(i-1) }(10)],
             [ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 ]);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
