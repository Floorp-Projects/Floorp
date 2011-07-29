function f(code) {
    a = code.replace(/s/, "");
    wtt = a
    code = code.replace(/\/\*DUPTRY\d+\*\//, function(k) {
        n = parseInt(k.substr(8), 0);
        return g("try{}catch(e){}", n)
    });
    f = eval("(function(){" + code + "})")
    if (typeof disassemble == 'function') {
        disassemble("-r", f)
    }
}
function g(s, n) {
    if (n == 0) {
        return s
    }
    s2 = s + s
    r = n % 2
    d = (n - r) / 2
    m = g(s2, d)
    return r ? m + s : m
}
f("switch(''){default:break;/*DUPTRY525*/}")
