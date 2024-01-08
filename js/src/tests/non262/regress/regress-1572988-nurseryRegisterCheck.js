// |reftest| slow skip-if(!this.hasOwnProperty("oomTest"))

// Bug 1572988: Make a bunch of nursery ropes and flatten them with oomTest.
// The goal is to get an OOM while flattening that makes registering the
// string's malloc buffer with the nursery fail, triggering an assertion when
// it gets tenured.

var x = 0;
var N = 1000; // This failed most of the time on my linux64 box.

// But it can time out on the slower machines.
if (this.getBuildConfiguration &&
    (getBuildConfiguration("simulator") || getBuildConfiguration("arm") ||
     getBuildConfiguration("android"))) {
  N = 10;
}

function makeString() {
    x += 1;
    const extensible = ensureLinearString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa" + x);
    return ensureLinearString(newRope(extensible, "bbbbbbbbbbbbbbb"));
}

function f(arr) {
    for (let i = 0; i < N; i++)
        arr.push(makeString());
    return arr;
}

var globalStore = [];
function ff() {
  globalStore.push(f([]));
}

if (this.oomTest)
  oomTest(ff);

reportCompare(true, true, "nursery malloc buffer registration failure");
