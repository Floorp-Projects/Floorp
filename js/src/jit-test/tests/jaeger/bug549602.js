version(180)
function f1(code) {
    var c
    var t = code.replace(/s/, "")
    var f = new Function(code)
    var o
    e = v = f2(f, c)
}
function f2(f, e) {
    try {
        a = f()
    } catch(r) {
        var r = g()
    }
}
g1 = [{
    text: "(function sum_slicing(array){return array==0?0:a+sum_slicing(array.slice(1))})",
    test: function (f) {
        f([, 2]) == ""
    }
}];
(function () {
    for (var i = 0; i < g1.length; ++i) {
        var a = g1[i]
        var text = a.text
        var f = eval(text.replace(/@/, ""))
        if (a.test(f)) {}
    }
}())
f1("for(let a=0;a<6;a++){print([\"\"].some(function(){false>\"\"}))}")

