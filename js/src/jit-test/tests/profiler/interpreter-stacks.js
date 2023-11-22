// |jit-test| --no-blinterp
// Disable Baseline Interpreter and JITs because we only read information about
// C++ Interpreter profiling frames.

enableGeckoProfilingWithSlowAssertions();

function assertStack(stack, expected) {
    assertEq(stack.length, expected.length);
    for (let i = 0; i < stack.length; i++) {
        // Use |split| to get rid of the file name.
        assertEq(stack[i].dynamicString.split(" (")[0], expected[i]);
    }
}

// We currently don't push entries for frames that were already on the stack
// when the profiler was enabled.
assertStack(readGeckoInterpProfilingStack(), []);

function testBasic() {
    let g1 = function() {
        return readGeckoInterpProfilingStack();
    }
    let f1 = () => g1();
    assertStack(f1(), ["testBasic", "f1", "g1"]);

    // Test non-function frames.
    assertStack(evaluate("eval(`(function foo() { return readGeckoInterpProfilingStack(); })()`)"),
                ["testBasic", "@evaluate", "@evaluate line 1 > eval:1:1", "foo"]);
}
testBasic();
testBasic();

function testThrow() {
    let stacks = [];
    let thrower = function() {
        stacks.push(readGeckoInterpProfilingStack());
        throw 1;
    };
    let catcher = function() {
        try {
            thrower();
        } catch (e) {
            stacks.push(readGeckoInterpProfilingStack());
        }
    };
    catcher();
    assertEq(stacks.length, 2);
    assertStack(stacks[0], ["testThrow", "catcher", "thrower"]);
    assertStack(stacks[1], ["testThrow", "catcher"]);
}
testThrow();
testThrow();

function testSelfHosted() {
    let stacks = [1, 2, 3].map(function() {
        return readGeckoInterpProfilingStack();
    });
    assertEq(stacks.length, 3);
    for (var stack of stacks) {
        assertStack(stack, ["testSelfHosted", "map", "testSelfHosted/stacks<"]);
    }
}
testSelfHosted();
testSelfHosted();

function testGenerator() {
    let stacks = [];
    let generator = function*() {
        stacks.push(readGeckoInterpProfilingStack());
        yield 1;
        stacks.push(readGeckoInterpProfilingStack());
        yield 2;
        stacks.push(readGeckoInterpProfilingStack());
    };
    for (let x of generator()) {}
    assertStack(readGeckoInterpProfilingStack(), ["testGenerator"]);

    assertEq(stacks.length, 3);
    for (var stack of stacks) {
        assertStack(stack, ["testGenerator", "next", "generator"]);
    }
}
testGenerator();
testGenerator();

async function testAsync() {
    let stacks = [];
    let asyncFun = async function() {
        stacks.push(readGeckoInterpProfilingStack());
        await 1;
        stacks.push(readGeckoInterpProfilingStack());
    };
    await asyncFun();
    assertStack(readGeckoInterpProfilingStack(), ["AsyncFunctionNext", "testAsync"]);

    assertEq(stacks.length, 2);
    assertStack(stacks[0], ["testAsync", "asyncFun"]);
    assertStack(stacks[1], ["AsyncFunctionNext", "asyncFun"]);
}
testAsync();
drainJobQueue();
testAsync();
drainJobQueue();

disableGeckoProfiling();
