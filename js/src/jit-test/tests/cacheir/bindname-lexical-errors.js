const x = 1;
function testConst() {
    for (var i = 0; i < 20; i++) {
        try {
            x = 2;
        } catch (e) {
            continue;
        }
        throw "Fail1";
    }
    assertEq(x, 1);
}
testConst();

function testUninit() {
    for (var i = 0; i < 20; i++) {
        try {
            y = 2;
        } catch (e) {
            continue;
        }
        throw "Fail2";
    }
}
testUninit();
let y;
