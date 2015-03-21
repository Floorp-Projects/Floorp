var x = "outer";

setLazyParsingEnabled(false);
{
    let x = "inner";
    eval("function g() { assertEq(x, 'inner');} g()");
}
setLazyParsingEnabled(true);

{
    let x = "inner";
    eval("function h() { assertEq(x, 'inner');} h()");
}

setLazyParsingEnabled(false);
with ({}) {
    let x = "inner";
    eval("function i() { assertEq(x, 'inner');} i()");
}
setLazyParsingEnabled(true);

with ({}) {
    let x = "inner";
    eval("function j() { assertEq(x, 'inner');} j()");
}

setLazyParsingEnabled(false);
(function () {
    var x = "inner";
    eval("function k() { assertEq(x, 'inner');} k()");
})();
setLazyParsingEnabled(true);

(function () {
    let x = "inner";
    eval("function l() { assertEq(x, 'inner');} l()");
})();
