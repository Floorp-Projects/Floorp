// |jit-test| skip-if: !wasmGcEnabled()

// Parsing and resolving.

var text = `(module
      (type $cons (struct
                   (field $car i32)
                   (field $cdr (ref null $cons))))

      (type $odd (struct
                  (field $odd.x i32)
                  (field $to_even (ref null $even))))

      (type $even (struct
                   (field $even.x i32)
                   (field $to_odd (ref null $odd))))

      ;; Use eqref on the API since struct types cannot be exposed outside the module yet.

      (import "m" "f" (func $imp (param eqref) (result eqref)))

      ;; The bodies do nothing since we have no operations on structs yet.
      ;; Note none of these functions are exported, as they use Ref types in their signatures.

      (func (param (ref null $cons)) (result i32)
       (i32.const 0))

      (func $cdr (param $p (ref null $cons)) (result (ref null $cons))
       (local $l (ref null $cons))
       ;; store null value of correct type
       (local.set $l (ref.null $cons))
       ;; store local of correct type
       (local.set $l (local.get $p))
       ;; store call result of correct type
       (local.set $l (call $cdr (local.get $p)))
       ;; TODO: eventually also a test with global.get
       ;; blocks and if with result type
       (block (result (ref null $cons))
        (if (result (ref null $cons)) (i32.eqz (i32.const 0))
            (unreachable)
            (ref.null $cons))))

      (func (param (ref null $even)) (result (ref null $odd))
       (ref.null $odd))

      (func (param (ref null $odd)) (result (ref null $even))
       (ref.null $even))

      (func (param (ref null $cons))
       (call $cdr (local.get 0))
       drop
       (call $imp (local.get 0))
       drop)

      (func (param (ref null $cons))
       (drop (ref.eq (local.get 0) (ref.null $cons)))
       (drop (ref.eq (ref.null $cons) (local.get 0)))
       (drop (ref.eq (local.get 0) (ref.null $cons)))
       (drop (ref.eq (ref.null $cons) (local.get 0))))
     )`;

// Validation

wasmValidateText(text);

// ref.is_null should work on any reference type

new WebAssembly.Module(wasmTextToBinary(`
(module
 (type $s (struct))
 (func $null (param (ref null $s)) (result i32)
   (ref.is_null (local.get 0))))
`))

// Automatic upcast to eqref

new WebAssembly.Module(wasmTextToBinary(`
(module
 (type $s (struct (field i32)))
 (func $f (param (ref null $s)) (call $g (local.get 0)))
 (func $g (param eqref) (unreachable)))
`));

// Misc failure modes

assertErrorMessage(() => wasmEvalText(`
(module
 (func (param (ref null $odd)) (unreachable)))
`),
SyntaxError, /failed to find type/);

// Ref type mismatch in parameter is allowed through the prefix rule
// but not if the structs are incompatible.

wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field i32)))
 (func $f (param (ref null $s)) (unreachable))
 (func $g (param (ref null $t)) (call $f (local.get 0)))
)`);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field f32))) ;; Incompatible type
 (func $f (param (ref null $s)) (unreachable))
 (func $g (param (ref null $t)) (call $f (local.get 0)))
)`),
WebAssembly.CompileError, /expression has type \(ref null.*\) but expected \(ref null.*\)/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field (mut i32)))) ;; Incompatible mutability
 (func $f (param (ref null $s)) (unreachable))
 (func $g (param (ref null $t)) (call $f (local.get 0)))
)`),
WebAssembly.CompileError, /expression has type \(ref null.*\) but expected \(ref null.*\)/);

// Ref type mismatch in assignment to local but the prefix rule allows
// the assignment to succeed if the structs are the same.

wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field i32)))
 (func $f (param (ref null $s)) (local (ref null $t)) (local.set 1 (local.get 0))))
`)

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field f32)))
 (func $f (param (ref null $s)) (local (ref null $t)) (local.set 1 (local.get 0))))
`),
WebAssembly.CompileError, /expression has type \(ref null.*\) but expected \(ref null.*\)/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field (mut i32))))
 (func $f (param (ref null $s)) (unreachable))
 (func $g (param (ref null $t)) (call $f (local.get 0)))
)`),
WebAssembly.CompileError, /expression has type \(ref null.*\) but expected \(ref null.*\)/);

// Ref type mismatch in return but the prefix rule allows the return
// to succeed if the structs are the same.

wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field i32)))
 (func $f (param (ref null $s)) (result (ref null $t)) (local.get 0)))
`);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field f32)))
 (func $f (param (ref null $s)) (result (ref null $t)) (local.get 0)))
`),
WebAssembly.CompileError, /expression has type \(ref null.*\) but expected \(ref null.*\)/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (type $t (struct (field (mut i32))))
 (func $f (param (ref null $s)) (result (ref null $t)) (local.get 0)))
`),
WebAssembly.CompileError, /expression has type \(ref null.*\) but expected \(ref null.*\)/);

// Ref type can't reference a function type

assertErrorMessage(() => wasmEvalText(`
(module
 (type $x (func (param i32)))
 (func $f (param (ref null $x)) (unreachable)))
`),
WebAssembly.CompileError, /does not reference a gc type/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type (func (param i32)))
 (func $f (param (ref null 0)) (unreachable)))
`),
WebAssembly.CompileError, /does not reference a gc type/);

// No automatic downcast from eqref

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field i32)))
 (func $f (param eqref) (call $g (local.get 0)))
 (func $g (param (ref null $s)) (unreachable)))
`),
WebAssembly.CompileError, /expression has type eqref but expected \(ref null.*\)/);
