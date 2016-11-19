// Make sure the real class name is not exposed by Errors.
function assertThrowsObjectError(f) {
    try {
        f();
        assertEq(true, false);
    } catch (e) {
        assertEq(e instanceof TypeError, true);
        assertEq(e.message.includes("called on incompatible Object"), true);
    }
}

assertThrowsObjectError(() => Map.prototype.has(0));
assertThrowsObjectError(() => Set.prototype.has(0));
assertThrowsObjectError(() => WeakMap.prototype.has({}));
assertThrowsObjectError(() => WeakSet.prototype.has({}));
assertThrowsObjectError(() => Date.prototype.getSeconds());
