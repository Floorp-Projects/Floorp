
load("verify.js");


function f1(a = 1) { return a; }

function f2(a, b = 1) { return a + b; }

function f3(a, b = 1, c = 2) { return a + b + c; }

function f4(a, b, c) { return a + b + c; }



function f5(| a) { return a; }

function f6(| a = 1) { return a; }

function f7(| 'q' a = 1) { return a; }


function f8(a,| b) { return a + b; }

function f9(a,| 'q' b) { return a + b; }

function f10(a,| 'q' b = 1) { return a + b; }


function f11(a, b, c, ...) { return a + b + c; }

function f12(a, b, c, ...d) { return a + b + c + d[0]; }

function f13(a, b, c, ...d) { return a + b + c + d[0] + d[1]; }


function f14(| a, b, c) { return a + b - c; }

function f15(| a, b = 2, c) { return a + b - c; }



verify( f1(), 1);
verify( f1(2), 2);

verify( f2(1), 2);
verify( f2(1,2), 3);

verify( f3(1), 4);
verify( f3(1,2), 5);
verify( f3(1,2,3), 6);

verify( f4(1,2,3), 6);


verify( f5(a:1), 1);

verify( f6(), 1);
verify( f6(a:2), 2);

verify( f7(a:1), 1);
verify( f7(q:1), 1);
verify( f7(), 1);

verify( f8(1, b:2), 3);

verify( f9(1, b:1), 2);
verify( f9(1, q:1), 2);

verify( f10(1), 2);
verify( f10(1, b:2), 3);

verify( f11(1, 2, 3), 6);

verify( f12(1, 2, 3, 4), 10);
verify( f12(1, 2, 3, 4, 5), 10);

verify( f13(1, 2, 3, 4, 5), 15);
verify( f13(1, 2, 3, 4, 5, 6), 15);

verify( f14(a:4, b:2, c:3), 3);
verify( f14(c:3, b:2, a:4), 3);
verify( f14(b:2, a:4, c:3), 3);

verify( f15(a:4, c:3), 3);

