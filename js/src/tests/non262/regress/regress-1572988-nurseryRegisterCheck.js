// |reftest| skip-if(!this.hasOwnProperty("oomTest"))

// Bug 1572988: Make a bunch of nursery ropes and flatten them with oomTest.
// The goal is to get an OOM while flattening that makes registering the
// string's malloc buffer with the nursery fail, triggering an assertion when
// it gets tenured.

var x = 0;
function makeString() {
    x += 1;
    const extensible = ensureFlatString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa" + x);
    return ensureFlatString(newRope(extensible, "bbbbbbbbbbbbbbb"));
}

function f(arr) {
    for (let i = 0; i < 1000; i++)
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
