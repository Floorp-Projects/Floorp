var countG = 0;
function g() {
    switch(countG++) {
     case 0: return 42;
     case 1: return "yo";
     case 2: return {};
    }
}

var countFault = 0;
function uceFault() {
    if (countFault++ == 4)
        uceFault = function() { return true }
    return false;
}

function f() {
    var x = !g();
    if (uceFault() || uceFault()) {
        assertEq(x, false);
        return 0;
    }
    return 1;
}

f();
f();
f();
