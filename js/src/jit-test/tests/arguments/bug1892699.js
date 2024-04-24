async function a(x, y, z) {
    if (arguments.length !== 3) {
        throw "Wrong output";
    }
    await x;
    if (arguments.length !== 3) {
        throw "Wrong output";
    }
    await y;
    if (arguments.length !== 3) {
        throw "Wrong output";
    }
    await z;
}
const p = a(3, 4, 5);
p.then(() => { assertEq(true, true) })
