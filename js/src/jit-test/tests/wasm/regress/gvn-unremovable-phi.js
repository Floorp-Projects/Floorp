wasmEvalText(`(module
  (type $type0 (func (param i32)))
  (func $f (param $p i32)
    (local $x i32) (local $y i32)
    loop $top
      local.get $x
      local.get $p
      local.get $x
      br_if $top
      i32.const 1
      tee_local $p
      local.get $y
      local.set $x
      i32.add
      call $f
      br_if $top
      return
    end
  )
)`);
