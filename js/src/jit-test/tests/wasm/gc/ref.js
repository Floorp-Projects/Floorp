// |jit-test| skip-if: !wasmReftypesEnabled() || !wasmGcEnabled()

// Parsing and resolving.

var bin = wasmTextToBinary(
    `(module
      (type $cons (struct
                   (field $car i32)
                   (field $cdr (ref opt $cons))))

      (type $odd (struct
                  (field $odd.x i32)
                  (field $to_even (ref opt $even))))

      (type $even (struct
                   (field $even.x i32)
                   (field $to_odd (ref opt $odd))))

      ;; Use anyref on the API since struct types cannot be exposed outside the module yet.

      (import "m" "f" (func $imp (param anyref) (result anyref)))

      ;; The bodies do nothing since we have no operations on structs yet.
      ;; Note none of these functions are exported, as they use Ref types in their signatures.

      (func (param (ref opt $cons)) (result i32)
       (i32.const 0))

      (func $cdr (param $p (ref opt $cons)) (result (ref opt $cons))
       (local $l (ref opt $cons))
       ;; store null value of correct type
       (local.set $l (ref.null))
       ;; store local of correct type
       (local.set $l (local.get $p))
       ;; store call result of correct type
       (local.set $l (call $cdr (local.get $p)))
       ;; TODO: eventually also a test with global.get
       ;; blocks and if with result type
       (block (result (ref opt $cons))
        (if (result (ref opt $cons)) (i32.eqz (i32.const 0))
            (unreachable)
            (ref.null))))

      (func (param (ref opt $even)) (result (ref opt $odd))
       (ref.null))

      (func (param (ref opt $odd)) (result (ref opt $even))
       (ref.null))

      (func (param (ref opt $cons))
       (call $cdr (local.get 0))
       drop
       (call $imp (local.get 0))
       drop)

      (func (param (ref opt $cons))
       (drop (ref.eq (local.get 0) (ref.null)))
       (drop (ref.eq (ref.null) (local.get 0)))
       (drop (ref.eq (local.get 0) (ref.null)))
       (drop (ref.eq (ref.null) (local.get 0))))
     )`);

// Validation

assertEq(WebAssembly.validate(bin), true);

// ref.is_null should work on any reference type

new WebAssembly.Module(wasmTextToBinary(`
(module
 (type $s (struct))
 (func $null (param (ref opt $s)) (result i32)
   (ref.is_null (local.get 0))))
`))

// Automatic upcast to anyref

new WebAssembly.Module(wasmTextToBinary(`
(module
 (type $s (struct (field i32)))
 (func $f (param (ref opt $s)) (call $g (local.get 0)))
 (func $g (param anyref) (unreachable)))
`));

// Misc failure modes

assertErrorMessage(() => wasmEvalText(`
(module
 (func (param (ref opt $odd)) (unreachable)))
`),
SyntaxError, /failed to find type/);

// Ref type mismatch in parameter is allowed through the prefix rule
// but not if the structs are incompatible.

wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field i32)))
 (func $f (param (ref opt $s)) (unreachable))
 (func $g (param (ref opt $t)) (call $f (local.get 0)))
)`);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field f32))) ;; Incompatible type
 (func $f (param (ref opt $s)) (unreachable))
 (func $g (param (ref opt $t)) (call $f (local.get 0)))
)`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field (mut i32)))) ;; Incompatible mutability
 (func $f (param (ref opt $s)) (unreachable))
 (func $g (param (ref opt $t)) (call $f (local.get 0)))
)`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

// Ref type mismatch in assignment to local but the prefix rule allows
// the assignment to succeed if the structs are the same.

wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field i32)))
 (func $f (param (ref opt $s)) (local (ref opt $t)) (local.set 1 (local.get 0))))
`)

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field f32)))
 (func $f (param (ref opt $s)) (local (ref opt $t)) (local.set 1 (local.get 0))))
`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field (mut i32))))
 (func $f (param (ref opt $s)) (unreachable))
 (func $g (param (ref opt $t)) (call $f (local.get 0)))
)`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

// Ref type mismatch in return but the prefix rule allows the return
// to succeed if the structs are the same.

wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field i32)))
 (func $f (param (ref opt $s)) (result (ref opt $t)) (local.get 0)))
`);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field f32)))
 (func $f (param (ref opt $s)) (result (ref opt $t)) (local.get 0)))
`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field (mut i32))))
 (func $f (param (ref opt $s)) (result (ref opt $t)) (local.get 0)))
`),
WebAssembly.CompileError, /expression has type ref.*but expected ref/);

// Ref type can't reference a function type

assertErrorMessage(() => wasmEvalText(`
(module
 (type $x (func (param i32)))
 (func $f (param (ref opt $x)) (unreachable)))
`),
WebAssembly.CompileError, /ref does not reference a struct type/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type (func (param i32)))
 (func $f (param (ref opt 0)) (unreachable)))
`),
WebAssembly.CompileError, /does not reference a struct type/);

// No automatic downcast from anyref

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (func $f (param anyref) (call $g (local.get 0)))
 (func $g (param (ref opt $s)) (unreachable)))
`),
WebAssembly.CompileError, /expression has type anyref but expected ref/);
