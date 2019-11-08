wasmValidateText(`
  (module
    (func (result i32)
      (i32.add
        (block (result i32 i32)
          (i32.const 32)
          (i32.const 10)))))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32) (i32.const 10)
      (block (param i32 i32) (result i32 i32))
      (i32.add)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (block (param i32) (result i32 i32)
        (i32.const 10))
      (i32.add)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.add
        (loop (result i32 i32)
          (i32.const 32)
          (i32.const 10)))))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32) (i32.const 10)
      (loop (param i32 i32) (result i32 i32))
      (i32.add)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (loop (param i32) (result i32 i32)
        (i32.const 10))
      (i32.add)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.add
        (block (result i32 i32)
          (i32.const 32)
          (i32.const 10)
          (br 0)))))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (i32.const 10)
      (block (param i32 i32) (result i32 i32)
        (br 0))
      (i32.add)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (block (param i32) (result i32 i32)
        (i32.const 10)
        (br 0))
      (i32.add)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.add
        (block (result i32 i32)
          (i32.const 32)
          (i32.const 10)
          (block (param i32 i32) (result i32 i32)
            (br 1))))))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (i32.const 10)
      (block (param i32 i32) (result i32 i32)
        (block (param i32 i32) (result i32 i32)
          (br 1)))
      (i32.add)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (i32.const 10)
      (block (param i32 i32) (result i32 i32)
        (block (param i32 i32) (result i32 i32)
          (br 0)))
      (i32.add)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (block (param i32) (result i32 i32)
        (i32.const 10)
        (block (param i32 i32) (result i32 i32)
          (br 1)))
      (i32.add)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 1)
      (if (param i32 i32) (result i32)
          (then (i32.add))
          (else (i32.sub)))))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 1)
      (if (param i32 i32) (result i32 i32)
          (then
            (drop)
            (drop)
            (i32.const 10)
            (i32.const 32)))
      (i32.sub)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 1)
      (if (param i32 i32) (result i32 i32)
          (then (return)))
      (i32.sub)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 1)
      (if (param i32 i32) (result i32 i32)
          (then)
          (else (return)))
      (i32.sub)))`);

wasmValidateText(`
  (module
    (func (result i32)
      (local $i i32)
      (i32.const 0) ;; sum
      (i32.const 10) ;; i
      (loop $loop (param i32 i32) (result i32)
        (local.tee $i)
        (i32.add) ;; sum = i + sum
        (i32.sub (local.get $i) (i32.const 1))
        (local.tee $i)
        (local.get $i)
        (br_if $loop (i32.eqz))
        (drop))))`);

// Tests the encoding: with more than 64 function types, the block type encoded
// as an uleb will create a confusion with the valtype; an sleb is required.

wasmValidateText(`
  (module
    ${(function () {
        s = "";
        for ( let i=0; i < 64; i++ ) {
          let t = [];
          for ( let j=0; j < 6; j++ )
            t.push(i & (1 << j) ? "i32" : "f32");
          s += "(func (param " + t.join(" ") + "))\n";
        }
        return s;
       })()}
    (func (result i32)
      (i32.add
        (block (result i32 i32)
          (i32.const 32)
          (i32.const 10)))))`);
