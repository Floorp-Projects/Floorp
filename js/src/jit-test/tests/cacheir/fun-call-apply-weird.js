// |jit-test| --fast-warmup

// Function with overridden call/apply (scripted).
function funOverridden1(x, y) { return x + y; }
funOverridden1.call = x => x + 1;
funOverridden1.apply = x => x + 2;

// Function with overridden call/apply (native).
function funOverridden2(x, y) { return x + y; }
funOverridden2.call = Math.abs;
funOverridden2.apply = Math.abs;

// Function with call/apply properties with other names.
function funOverridden3(x, y) { return x + y; }
funOverridden3.myCall = Function.prototype.call;
funOverridden3.myApply = Function.prototype.apply;

function f() {
    var arr = [1, 2];
    for (var i = 0; i < 100; i++) {
        assertEq(funOverridden1.call(i, i), i + 1);
        assertEq(funOverridden1.apply(i, i), i + 2);

        assertEq(funOverridden2.call(i, i), i);
        assertEq(funOverridden2.apply(i, i), i);

        assertEq(funOverridden3.myCall(null, i, i), i + i);
        assertEq(funOverridden3.myApply(null, arr), 3);
    }
}
f();
