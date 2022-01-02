
function f() {
    try {} catch (x) {
        return;
    } finally {
        null.x;
    }
}

try {
    f();
} catch (x) {}
