load(libdir + "asserts.js");
var ieval = eval;
var offenders = [["..."], ["...rest"," x"], ["...rest", "[x]"],
                 ["...rest", "...rest2"]];
for (var arglist of offenders) {
    assertThrowsInstanceOf(function () {
        ieval("function x(" + arglist.join(", ") + ") {}");
    }, SyntaxError);
    assertThrowsInstanceOf(function () {
        Function.apply(null, arglist.concat("return 0;"));
    }, SyntaxError);
}