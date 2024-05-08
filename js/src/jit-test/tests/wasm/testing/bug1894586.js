var it = 100;
function f() {
  if (--it < 0) {
    return;
  }
  wasmDumpIon(
    wasmTextToBinary(
      "(type $x (struct))(global $g (ref null $x) ref.null $x)(func $h)"
    )
  );
  oomTest(f);
}
f();
