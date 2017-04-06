var max = 40;
setJitCompilerOption("ion.warmup.trigger", max - 10);

function selfhosted() {
    if (typeof getSelfHostedValue === "undefined")
        return;

    var NewArrayIterator = getSelfHostedValue("NewArrayIterator");
    var iter = NewArrayIterator();
    bailout();
    // assertRecoveredOnBailout(iter, true);
}

function iterator(i) {
    var array = [1, i];
    var iter = array[Symbol.iterator]();
    assertEq(iter.next().value, 1);
    bailout();
    // This sometimes fails
    // assertRecoveredOnBailout(iter, true);
    var result = iter.next();
    assertEq(result.value, i);
    assertEq(result.done, false);
    assertEq(iter.next().done, true);
}

function forof(i) {
    var array = [1, i];
    var first = true;

    for (var x of array) {
        if (first) {
            assertEq(x, 1);
            bailout();
            first = false;
        } else {
            assertEq(x, i);
        }
    }
}

var data = {
  a: 'foo',
  b: {c: 'd'},
  arr: [1, 2, 3]
};

function fn() {
  var {a, b:{c:b}, arr:[, c]} = data;
  return c;
}

function destructuring() {
    for (var i = 0; i < max; i++)
        assertEq(fn(), 2);
}

for (var i = 0; i < max; i++) {
    selfhosted();
    iterator(i);
    forof(i);
    destructuring();
}
