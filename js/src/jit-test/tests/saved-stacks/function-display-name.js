// Test the functionDisplayName of SavedFrame instances.

function uno() { return dos(); }
const dos = () => tres.quattro();
let tres = {};
tres.quattro = () => saveStack()

const frame = uno();

assertEq(frame.functionDisplayName, "tres.quattro");
assertEq(frame.parent.functionDisplayName, "dos");
assertEq(frame.parent.parent.functionDisplayName, "uno");
assertEq(frame.parent.parent.parent.functionDisplayName, null);

assertEq(frame.parent.parent.parent.parent, null);

