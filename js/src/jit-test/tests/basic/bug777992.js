verifyprebarriers()
x = []
function z() {}
Object.defineProperty(x, 2, {
    value: z
})
gczeal(2, 2)
y = x.slice(2)
y.e = (function() {})
