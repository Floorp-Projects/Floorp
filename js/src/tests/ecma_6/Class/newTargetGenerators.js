function *generatorNewTarget(expected) {
    assertEq(new.target, expected);
    assertEq(eval('new.target'), expected);
    assertEq((() => new.target)(), expected);
    yield (() => new.target);
}

const ITERATIONS = 25;

for (let i = 0; i < ITERATIONS; i++)
    assertEq(generatorNewTarget(undefined).next().value(), undefined);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
