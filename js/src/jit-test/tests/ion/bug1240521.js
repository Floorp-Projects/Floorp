// |jit-test| allow-oom

var egc = 138;
function SwitchTest(value) {
    switch (value) {
        case 0:
            break
        case new Number:
            result = 8
        case oomAfterAllocations(egc):
    }
}
!(SwitchTest(4) === 4);
!(SwitchTest(true) === 2);
