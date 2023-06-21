// |jit-test| skip-if: !wasmGcEnabled()

// Validate rules for function subtyping:
// - Same number of parameters and results
// - Function return types are covariant
// - Function parameter types are contravariant
wasmValidateText(
`(module
    (type $A (sub (struct (field i32))))
    (type $B (sub $A (struct (field i32) (field i32))))
    (type $C (sub $B (struct (field i32) (field i32) (field i64))))
    (type $D (sub $B (struct (field i32) (field i32) (field f32))))

    ;; Same types (invariant)
    (type $func1 (sub (func (param (ref $A) (ref $A)) (result (ref $C)))))
    (type $func2 (sub $func1 (func (param (ref $A) (ref $A)) (result (ref $C)))))

    ;; Covariant return types are valid
    (type $func3 (sub (func (param (ref $A) (ref $A)) (result (ref $B)))))
    (type $func4 (sub $func3 (func (param (ref $A) (ref $A)) (result (ref $C)))))
    (type $func5 (sub (func (param (ref $A) (ref $A)) (result (ref $A) (ref $B)))))
    (type $func6 (sub $func5 (func (param (ref $A) (ref $A)) (result (ref $D) (ref $C)))))

    ;; Contravariant parameter types are valid
    (type $func7 (sub (func (param (ref $A) (ref $C)) (result (ref $C)))))
    (type $func8 (sub $func7 (func (param (ref $A) (ref $B)) (result (ref $C)))))
    (type $func9 (sub (func (param (ref $D) (ref $C)) (result (ref $C)))))
    (type $func10 (sub $func9 (func (param (ref $A) (ref $B)) (result (ref $C)))))

    ;; Mixing covariance and contravariance
    (type $func11 (sub (func (param (ref $D) (ref $C)) (result (ref $A)))))
    (type $func12 (sub $func11 (func (param (ref $A) (ref $B)) (result (ref $C)))))
)
`);

// Validate that incorrect subtyping examples are failing as expected
const typeError = /incompatible super type/;

var code =`
(module
    (type $A (struct (field i32)))
    (type $B (sub $A (struct (field i32) (field i32))))
    (type $C (sub $B (struct (field i32) (field i32) (field i64))))
    (type $D (sub $B (struct (field i32) (field i32) (field f32))))

    ;; Not the same number of arguments/results
    (type $func1 (func (param (ref $A) (ref $A)) (result (ref $C))))
    (type $func2 (sub $func1 (func (param (ref $A) (ref $A)) (result (ref $C) (ref $A)))))
)`;

wasmFailValidateText(code, typeError);

code =`
(module
    (type $A (sub (struct (field i32))))
    (type $B (sub $A (struct (field i32) (field i32))))
    (type $C (sub $B (struct (field i32) (field i32) (field i64))))
    (type $D (sub $B (struct (field i32) (field i32) (field f32))))

    ;; Contravariant result types are invalid
    (type $func3 (sub (func (param (ref $A) (ref $A)) (result (ref $C)))))
    (type $func4 (sub $func3 (func (param (ref $A) (ref $A)) (result (ref $A)))))
)`;

wasmFailValidateText(code, typeError);

code =`
(module
    (type $A (sub (struct (field i32))))
    (type $B (sub $A (struct (field i32) (field i32))))
    (type $C (sub $B (struct (field i32) (field i32) (field i64))))
    (type $D (sub $B (struct (field i32) (field i32) (field f32))))

    ;; Covariant parameters are invalid
    (type $func5 (sub (func (param (ref $A) (ref $A)) (result (ref $C)))))
    (type $func6 (sub $func5 (func (param (ref $B) (ref $A)) (result (ref $C)))))
)`;

wasmFailValidateText(code, typeError);
