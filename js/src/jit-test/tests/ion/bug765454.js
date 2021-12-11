var seen = -1;

// Test to make sure the jits get the number of calls, and return value
// of setters correct. We should not be affected by whether the setter
// modifies its argument or returns some value.
function setter(x) {
    this.val = x;
    x = 255;
    bailout();
    seen++;
    assertEq(seen, this.val);
    return 5;
}

function F(){}
Object.defineProperty(F.prototype, "value" , ({set: setter}));

function test() {
    var obj = new F();
    var itrCount = 10000;
    for(var i = 0; i < itrCount; i++) {
        assertEq(obj.value = i, i);
        assertEq(obj.val, i);
    }
}
test();
