// This program crashes the ARM code generator because the machine code is
// longer than the 32MB range of ARM branch instructions.
//
// Baseline should not attempt to compile the script.

i = 1;
function test(s) eval("line0 = Error.lineNumber\ndebugger\n" + s);
function repeat(s) {
        return Array(65 << 13).join(s)
}
long_expr = repeat(" + i")
long_throw_stmt = long_expr;
test(long_throw_stmt);
