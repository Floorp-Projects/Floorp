// |jit-test| allow-oom;
function h(f, inputs) {
    for (var j = 0; j < 99; ++j) {
        for (var k = 0; k < 99; ++k) {
            oomAfterAllocations(10)
        }
    }
}
m = function(y) {};
h(m, []);
