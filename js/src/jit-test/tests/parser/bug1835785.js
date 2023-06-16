// |jit-test| allow-unhandlable-oom; allow-oom; skip-if: !('oomAtAllocation' in this)
function main() {
  this
  oomAtAllocation(7);
  var v7 = /a/
  disassemble();
}
main();
