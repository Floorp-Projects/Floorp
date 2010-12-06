// vim: set ts=4 sw=4 tw=99 et:

function F() {
    var T = { };
    try {
        throw 12;
    } catch (e) {
        T.x = 5;
        return T;
    }
}

assertEq((new F()).x, 5);

