try {
function g(code) {
    code = code.replace(/\/\*DUPTRY\d+\*\//, function(k) {
        var n = parseInt(k.substr(8), 10);
        return aa("try{}catch(e){}", n);
    });
    var f = new Function(code);
    f()
}
function aa(s, n) {
    if (n == 1) {
        return s;
    }
    var s2 = s + s;
    var r = n % 2;
    var d = (n - r) / 2;
    var m = aa(s2, d);
    return r ? m + s : m;
}
g("switch(x){default:case l:/*DUPTRY5338*/case 0:x}");
} catch (e) {}
