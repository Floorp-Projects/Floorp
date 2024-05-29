let countersBefore = getUseCounterResults();

class MyArray extends Array { }

function f() { return MyArray.from([1, 2, 3]) };
assertEq(f() instanceof MyArray, true);
for (var i = 0; i < 100; i++) { f(); }

let countersAfter = getUseCounterResults();

// The above code should have tripped the subclassing detection.
assertEq(countersAfter.SubclassingArrayTypeII > countersBefore.SubclassingArrayTypeII, true);

function f2() {
    return Array.from([1, 2, 3]);
}
f2();

let countersAfterNoChange = getUseCounterResults();
assertEq(countersAfter.SubclassingArrayTypeII, countersAfterNoChange.SubclassingArrayTypeII);
