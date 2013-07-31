// |jit-test| error:TypeError
gc();
var recursiveFunctions = [{
    text: "(function(){if(a){}g()})"
}];
(function testAllRecursiveFunctions() {
    for (var i = 0; i < recursiveFunctions.length; ++i) {
        var a = recursiveFunctions[i];
        eval(a.text.replace(/@/g, ""))
    }
})();
gcslice(2868);
Function("v={c:[{x:[[]],N:{x:[{}[d]]}}]}=minorgc(true)")()
