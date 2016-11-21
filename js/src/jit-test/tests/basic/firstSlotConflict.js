// |jit-test| need-for-each

(function(x) {
    function f1() { return 1; }
    function f2() { return 2; }
    function f3() { return 3; }
    function f4() { return 4; }
    var g = function () { return x; }
    var a = [f1, f2, f3, f4, g];
    for each (var v in a)
        v.adhoc = 42;   // Don't assertbotch in jsbuiltins.cpp setting g.adhoc
})(33);
