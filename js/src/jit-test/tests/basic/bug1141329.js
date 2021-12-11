x = []
Object.defineProperty(x, 7, {})
function f() {
    "use strict"
    x.length = {
        valueOf: function() {
            return 7
        }
    }
}
for (var m = 0; m < 9; m++) {
    try {
        f()
    } catch (e) {}
}
