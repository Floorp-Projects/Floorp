function g(s) {
    return eval(s)
}
f = g("(function(){({x:function::arguments})})")
f()
