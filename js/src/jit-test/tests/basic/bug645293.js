/* Don't assert. */
function f() {
    NaN++;
    --NaN;
    Infinity--;
    ++Infinity;
    undefined++;
    --undefined;
    ++Math;
    Math--;
}
f();