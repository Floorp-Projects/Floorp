var progress = "";

function* wrapNoThrow() {
    let iter = {
        [Symbol.iterator]() { return this; },
        next() { return { value: 10, iter: false }; },
        return() { progress += " throw"; throw "nonsense"; }
    };
    for (const i of iter)
        yield i;
}

function foo() {
    try {
        var badIter = wrapNoThrow();
        loop: for (var i of badIter) {
            progress += "outerloop";
            try {
                for (i of [1,2,3]) {
                    progress += " innerloop";
                    break loop;
                }
            } catch (e) { progress += " BAD CATCH"; }
        }
    } catch (e) { progress += " goodcatch"; }
}

foo();
assertEq(progress, "outerloop innerloop throw goodcatch");
