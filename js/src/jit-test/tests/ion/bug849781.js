function c0(i) { print(i) }
function c1() { c0.apply({}, arguments); }
function c2() { c1.apply({}, arguments); }
function c3(a) { c2(a); }
c3(1);
c3(1);
c3("");
c3("");
