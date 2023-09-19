// |jit-test| skip-if: !wasmGcEnabled()

const INSTRUCTIONS = [
  "struct.new $s",
  "struct.new_default $s",
  "struct.get $s 0",
  "struct.get_s $s 1",
  "struct.get_u $s 1",
  "struct.set $s 0",
  "array.new $a_unpacked",
  "array.new_fixed $a_unpacked 2",
  "array.new_default $a_unpacked",
  "array.new_data $a_data 0",
  "array.new_elem $a_elem 0",
  "array.get $a_unpacked",
  "array.get_s $a_packed",
  "array.get_u $a_packed",
  "array.set $a_unpacked",
  "array.copy $a_unpacked $a_unpacked",
  "array.len",
  "ref.i31",
  "i31.get_s",
  "i31.get_u",
  "ref.test structref",
  "ref.test (ref $s)",
  "ref.test nullref",
  "ref.test (ref $f)",
  "ref.test nullfuncref",
  "ref.test externref",
  "ref.test nullexternref",
  "ref.cast structref",
  "ref.cast (ref $s)",
  "ref.cast nullref",
  "ref.cast (ref $f)",
  "ref.cast nullfuncref",
  "ref.cast externref",
  "ref.cast nullexternref",
  "br_on_cast 0 anyref (ref $s)",
  "br_on_cast_fail 0 anyref (ref $s)",
  "extern.internalize",
  "extern.externalize",
];

for (let instruction of INSTRUCTIONS) {
  print(instruction);
  wasmEvalText(`(module
    (type $f (func))
    (type $s (struct (field (mut i32)) (field (mut i8))))
    (type $a_unpacked (array (mut i32)))
    (type $a_packed (array (mut i8)))
    (type $a_data (array (mut i32)))
    (type $a_elem (array (mut anyref)))
    (data "")
    (elem anyref)
    (func (result anyref)
      unreachable
      ${instruction}
      unreachable
    )
  )`);
}
