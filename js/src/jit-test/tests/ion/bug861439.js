
try {
    x = evalcx('')
    toSource = (function() {
        x = (new WeakMap).get(function() {})
    })
    valueOf = (function() {
        schedulegc(x)
    })
    this + ''
    for (v of this) {}
} catch (e) {}
gc()
this + 1
