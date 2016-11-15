load(libdir + "wasm.js");

wasmEvalText(`(module
  (type $type0 (func (param i32)))
  (func $f (param $p i32)
    (local $x i32) (local $y i32)
    loop $top
      get_local $x
      get_local $p
      get_local $x
      br_if $top
      i32.const 1
      tee_local $p
      get_local $y
      set_local $x
      i32.add
      call $f
      br_if $top
      return
    end
  )
)`);
