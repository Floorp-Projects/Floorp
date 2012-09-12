// IM: Test generated code
function neg(x) {
    return -x;
}
assertEq(neg(0), -0);
assertEq(neg(1), -1);
assertEq(neg(-1), 1);
assertEq(neg(-2147483648), 2147483648);
assertEq(neg(-1.3), 1.3);
assertEq(neg(1.45), -1.45);

// IM: Test constant folding
function neg2(){
    var x = 1;
    var y = -x;
    return y;
}
assertEq(neg2(), -1);
function neg3(){
    var x = 0;
    var y = -x;
    return y;
}
assertEq(neg3(), -0);
function neg4(){
    var x = -2147483648;
    var y = -x;
    return y;
}
assertEq(neg4(), 2147483648);
