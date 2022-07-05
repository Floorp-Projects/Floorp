let m1 = parseModule(`
function x() {
    return 1;
}
function y() {
    x = function() { return 2; };
}
export { x, y };
`);
moduleLink(m1);
moduleEvaluate(m1);

registerModule('m1', m1);

let m2 = parseModule(`
import {x, y} from "m1";

function test(expected) {
    for (var i = 0; i < 2000; i++) {
        if (i > 1900) {
            assertEq(x(), expected);
        }
    }
}
test(1);
y();
test(2);
`);
moduleLink(m2);
moduleEvaluate(m2);
