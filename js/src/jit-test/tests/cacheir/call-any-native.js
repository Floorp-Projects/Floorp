// Test non-constructor calls

var funcs = [Math.max, Math.min, Math.floor, Math.ceil, Math.sin,
             Math.cos, Math.tan, Math.log, Math.acos, Math.asin];

// Calculate expected values
var expected = [Math.max(0.5, 2), Math.min(0.5, 2),
                Math.floor(0.5, 2), Math.ceil(0.5, 2),
                Math.sin(0.5, 2), Math.cos(0.5, 2),
                Math.tan(0.5, 2), Math.log(0.5, 2),
                Math.acos(0.5, 2), Math.asin(0.5, 2)];

// Test a polymorphic call site
for (var n = 0; n < 50; n++) {
    for (var i = 0; i < funcs.length; i++) {
        assertEq(funcs[i](0.5, 2), expected[i]);
    }
}

// Test a polymorphic spread call site
var spreadinput = [0.5, 2];
for (var n = 0; n < 50; n++) {
    for (var i = 0; i < funcs.length; i++) {
        assertEq(funcs[i](...spreadinput), expected[i]);
    }
}

// Test constructors

function f1(x) {this[0] = x; this.length = 3;}
function f2(x) {this[0] = x; this.length = 3;}
function f3(x) {this[0] = x; this.length = 3;}
function f4(x) {this[0] = x; this.length = 3;}
function f5(x) {this[0] = x; this.length = 3;}
function f6(x) {this[0] = x; this.length = 3;}
function f7(x) {this[0] = x; this.length = 3;}
function f8(x) {this[0] = x; this.length = 3;}
function f9(x) {this[0] = x; this.length = 3;}

var constructors = [f1,f2,f3,f4,f5,f6,f7,f8,f9,Array];

// Test a polymorphic constructor site
for (var n = 0; n < 50; n++) {
    for (var i = 0; i < constructors.length; i++) {
        let x = new constructors[i](1,2,3);
        assertEq(x.length, 3);
        assertEq(x[0], 1);
    }
}

var constructorinput = [1,2,3];
// Test a polymorphic spread constructor site
for (var n = 0; n < 50; n++) {
    for (var i = 0; i < constructors.length; i++) {
        let x = new constructors[i](...constructorinput);
        assertEq(x.length, 3);
        assertEq(x[0], 1);
    }
}
