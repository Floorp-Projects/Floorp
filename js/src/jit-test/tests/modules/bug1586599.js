let m1 = parseModule(`
function x() {
    return 1;
}
function y() {
    x = function() { return 2; };
}
export { x, y };
`);
m1.declarationInstantiation();
m1.evaluation();

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
m2.declarationInstantiation();
m2.evaluation();
