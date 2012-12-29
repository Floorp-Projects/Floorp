Module = {};
var Runtime = {
    alignMemory: function alignMemory(size, quantum) {
	return Math.ceil((size) / (quantum ? quantum : 4)) * (quantum ? quantum : 4);
    },
}
function assert(condition, text) {
    throw text;
}
STACK_ROOT = STACKTOP = Runtime.alignMemory(1);
function _main() {
    var __stackBase__ = STACKTOP;
    var label;
    label = 2;
    while (1) {
	switch (label) {
          case 2:
            var $f = __stackBase__;
            var $1 = __stackBase__ + 12;
            var $2 = __stackBase__ + 24;
            var $3 = $f | 0;
            var $4 = $f + 4 | 0;
            var $5 = $f + 8 | 0;
            var $_0 = $1 | 0;
            var $_1 = $1 + 4 | 0;
            var $_2 = $1 + 8 | 0;
            var $j_012 = 0;
            label = 4;
            break;
          case 4:
            assertEq($_2, 24);
            if (($j_012 | 0) != 110) {
                var $j_012 = $j_012 + 1;
                break;
            }
            var $23 = $i_014 + 1 | 0;
            if (($23 | 0) != 110) {
                var $i_014 = $23;
		var $j_012 = 0;
                label = 4;
                break;
            }
          default:
            assert(0, "bad label: " + label);
	}
    }
}
try {
    _main(0, [], 0);
    assertEq(0, 1);
} catch(e) {
    assertEq(e, "bad label: 4");
}
