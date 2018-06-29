if (!wasmGcEnabled()) {
    assertErrorMessage(() => wasmEvalText(`(module (func (param (ref 0)) (unreachable)))`),
                       WebAssembly.CompileError, /bad type/);
    quit(0);
}

// Parsing and resolving

var bin = wasmTextToBinary(
    `(module
      (type $cons (struct
                   (field $car i32)
                   (field $cdr (ref $cons))))

      (type $odd (struct
                  (field $x i32)
                  (field $to_even (ref $even))))

      (type $even (struct
                   (field $x i32)
                   (field $to_odd (ref $odd))))

      ;; No globals of reference type yet.
      ;;
      ;;(import "m" "g" (global (ref $cons)))
      ;;
      ;;(global $glob (ref $cons) (ref.null $cons))

      (import "m" "f" (func $imp (param (ref $cons)) (result (ref $odd))))

      ;; The bodies do nothing since we have no operations on structs yet.

      (func (export "car") (param (ref $cons)) (result i32)
       (i32.const 0))

      (func $cdr (export "cdr") (param $p (ref $cons)) (result (ref $cons))
       (local $l (ref $cons))
       ;; store null value of correct type
       (set_local $l (ref.null (ref $cons)))
       ;; store local of correct type
       (set_local $l (get_local $p))
       ;; store call result of correct type
       (set_local $l (call $cdr (get_local $p)))
       ;; TODO: eventually also a test with get_global
       ;; blocks and if with result type
       (block (ref $cons)
        (if (ref $cons) (i32.eqz (i32.const 0))
            (unreachable)
            (ref.null (ref $cons)))))

      (func (export "odder") (param (ref $even)) (result (ref $odd))
       (ref.null (ref $odd)))

      (func (export "evener") (param (ref $odd)) (result (ref $even))
       (ref.null (ref $even)))

      (func (export "passer") (param (ref $cons))
       (call $cdr (get_local 0))
       drop
       (call $imp (get_local 0))
       drop)

     )`);

// Validation

assertEq(WebAssembly.validate(bin), true);

// ref.is_null should work on any reference type

new WebAssembly.Module(wasmTextToBinary(`
(module
 (type $s (struct))
 (func $null (param (ref $s)) (result i32)
   (ref.is_null (get_local 0))))
`))

// Automatic upcast to anyref

new WebAssembly.Module(wasmTextToBinary(`
(module
 (type $s (struct (field i32)))
 (func $f (param (ref $s)) (call $g (get_local 0)))
 (func $g (param anyref) (unreachable)))
`));

// Misc failure modes

assertErrorMessage(() => wasmEvalText(`
(module
  (func (param (ref $odd)) (unreachable)))
`),
SyntaxError, /Type label.*not found/);

// Ref type mismatch in parameter is allowed through the prefix rule
// but not if the structs are incompatible.

wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field i32)))
 (func $f (param (ref $s)) (unreachable))
 (func $g (param (ref $t)) (call $f (get_local 0)))
)`);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field f32))) ;; Incompatible type
 (func $f (param (ref $s)) (unreachable))
 (func $g (param (ref $t)) (call $f (get_local 0)))
)`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field (mut i32)))) ;; Incompatible mutability
 (func $f (param (ref $s)) (unreachable))
 (func $g (param (ref $t)) (call $f (get_local 0)))
)`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

// Ref type mismatch in assignment to local but the prefix rule allows
// the assignment to succeed if the structs are the same.

wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field i32)))
 (func $f (param (ref $s)) (local (ref $t)) (set_local 1 (get_local 0))))
`)

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field f32)))
 (func $f (param (ref $s)) (local (ref $t)) (set_local 1 (get_local 0))))
`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field (mut i32))))
 (func $f (param (ref $s)) (unreachable))
 (func $g (param (ref $t)) (call $f (get_local 0)))
)`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

// Ref type mismatch in return but the prefix rule allows the return
// to succeed if the structs are the same.

wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field i32)))
 (func $f (param (ref $s)) (result (ref $t)) (get_local 0)))
`);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field f32)))
 (func $f (param (ref $s)) (result (ref $t)) (get_local 0)))
`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field (mut i32))))
 (func $f (param (ref $s)) (result (ref $t)) (get_local 0)))
`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

// Ref type can't reference a function type

assertErrorMessage(() => wasmEvalText(`
(module
 (type $x (func (param i32)))
 (func $f (param (ref $x)) (unreachable)))
`),
SyntaxError, /Type label.*not found/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type (func (param i32)))
 (func $f (param (ref 0)) (unreachable)))
`),
WebAssembly.CompileError, /does not reference a struct type/);

// No automatic downcast from anyref

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (func $f (param anyref) (call $g (get_local 0)))
 (func $g (param (ref $s)) (unreachable)))
`),
WebAssembly.CompileError, /expression has type anyref but expected ref/);
