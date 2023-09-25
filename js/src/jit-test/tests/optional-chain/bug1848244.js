// Constant-folding should replace the ternary expression with one that is
// equally valid with optional chaining in the AST
function main() {
    (0 ? 0 : -1n)?.g;
}
main();
