// |jit-test| need-for-each

a2 = []
g = function() r
Object.defineProperty(a2, 0, {
    set: function() {}
})
for (var x = 0; x < 70; ++x) {
    Array.prototype.unshift.call(a2, g)
}
a2.length = 8
for each(e in [0, 0]) {
    Array.prototype.shift.call(a2)
}
