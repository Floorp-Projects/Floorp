load(libdir + "eqArrayHelper.js");

// These tests ensure that the gc related errors in bug 1744663 are resolved.

{
  let catchlessTry = `
            try
             (call $gc)
             (throw $exn)
            end`;
  let rethrow0 = `
            try
             (call $gc)
             (throw $exn)
            catch $exn
              (rethrow 0)
            end`;
  let rethrow1 = `
           try
             (throw $exn)
           catch_all
               try
                 (throw $exn)
               catch $exn
                 (rethrow 1)
               end
           end`;
  let delegate0 = `
           try
             (call $gc)
             (throw $exn)
           delegate 0`;
  let delegate1 = `
           (block
             try
               (call $gc)
               (throw $exn)
             delegate 1)`;
  let delegate0InCatch = `
            try
              (throw $exn)
            catch_all
                try
                  (call $gc)
                  (throw $exn)
                delegate 0
            end`;
  let delegate1InCatch = `
            try
              throw $exn
            catch_all
              try
                (call $gc)
                (throw $exn)
              delegate 1
            end`;

  let rethrowingBodies = [catchlessTry, rethrow0, delegate0, delegate1,
                          rethrow1, delegate0InCatch, delegate1InCatch];

  function rethrowingIndirectly(rethrowingBody) {
    let exports = wasmEvalText(
      `(module
         (tag $exn (export "exn"))
         (import "js" "gc" (func $gc))
         (func $throwExn (export "throwExn") ${rethrowingBody}))`,
      {js: {gc: () => { gc(); }}}
    ).exports;

    let mod =
        `(module
           (type $exnType (func))
           (type $indirectFunctype (func))
           (import "m" "exn" (tag $exn (type $exnType)))
           (import "m" "throwExn" (func $throwExn (type $indirectFunctype)))
           (table funcref (elem $throwExn))
           (func (export "testFunc") (result i32)
             try (result i32)
               (call_indirect (type $indirectFunctype) (i32.const 0))
                   (i32.const 0)
             catch $exn (i32.const 1)
             end
           )
         )`;

    let testFunction = wasmEvalText(mod, { m : exports}).exports.testFunc;
    assertEq(testFunction(), 1);
  };

  for (let rethrowingBody of rethrowingBodies) {
    //console.log("Calling rethrowingIndirectly with rethrowingBody = " + rethrowingBody);
    rethrowingIndirectly(rethrowingBody);
  }
}

// The full test case that caused the original failure.

{
  gczeal(2,1); // Collect after every allocation.

  let v128Type = " i32";
  let wrongV128 = "(i32.const 0)";
  let correctV128 = "(i32.const 1)";
  let checkV128Value = "";

  if (wasmSimdEnabled()) {
    v128Type = " v128";
    wrongV128 = "(v128.const i32x4 11 22 33 44)";
    correctV128 = "(v128.const i32x4 55 66 77 88)";
    checkV128Value = `;; Check the V128 value
               (v128.const i32x4 55 66 77 88)
               (i32x4.eq)
               (i32x4.all_true)`;
  }

  let exports = wasmEvalText(
    `(module
       (type $exnType (func (param i32 i64 f32 f64 externref ${v128Type})))
       (type $indirectFunctype (func (param i32 i64 f32 f64 externref ${v128Type})
                                     (result i32 i64 f32 f64 externref ${v128Type})))
       (tag $exn (export "exn") (type $exnType))
       (tag $emptyExn (export "emptyExn"))
       (func $throwExn (export "throwExn") (param i32 i64 f32 f64 externref ${v128Type})
                                           (result i32 i64 f32 f64 externref ${v128Type})
                                           (local $ifPredicate i32)
         (local.get 0) ;; i32
         (local.get 1) ;; i64
         (local.get 2) ;; f32
         (local.get 3) ;; f64
         (local.get 4) ;; ref
         (local.get 5) ;; v128 or i32
          try (param i32 i64 f32 f64 externref ${v128Type})
            (if (param i32 i64 f32 f64 externref ${v128Type})
              (local.get $ifPredicate)
              (then (throw $exn))
              (else (throw $exn)))
          catch $exn
            try (param i32 i64 f32 f64 externref ${v128Type})
              (throw $exn)
            catch_all (rethrow 1)
            end
          catch_all
          end
          unreachable)
       (func $throwEmptyExn (export "throwEmptyExn")
                              (param i32 i64 f32 f64 externref ${v128Type})
                              (result i32 i64 f32 f64 externref ${v128Type})
           (throw $emptyExn)
           unreachable)
         (func $returnArgs (export "returnArgs")
                           (param i32 i64 f32 f64 externref ${v128Type})
                           (result i32 i64 f32 f64 externref ${v128Type})
           (local.get 0)  ;; i32
           (local.get 1)  ;; i64
           (local.get 2)  ;; f32
           (local.get 3)  ;; f64
           (local.get 4)  ;; ref
           (local.get 5))
         (table (export "tab") funcref (elem $throwExn       ;; 0
                                             $throwEmptyExn  ;; 1
                                             $returnArgs))   ;; 2
         )`).exports;

  var mod =
      `(module
         (type $exnType (func (param i32 i64 f32 f64 externref ${v128Type})))
         (type $indirectFunctype (func (param i32 i64 f32 f64 externref ${v128Type})
                                 (result i32 i64 f32 f64 externref ${v128Type})))
         (import "m" "exn" (tag $exn (type $exnType)))
         (import "m" "emptyExn" (tag $emptyExn))
         (import "m" "throwExn" (func $throwExn (type $indirectFunctype)))
         (import "m" "throwEmptyExn"
                 (func $throwEmptyExn (type $indirectFunctype)))
         (import "m" "returnArgs"
                 (func $returnArgs (type $indirectFunctype)))
         (import "m" "tab" (table 3 funcref))
         (func (export "testFunc") (param $correctRef externref)
                                   (param $wrongRef externref)
                                   ;; The last i32 result is the v128 check.
                                   (result i32 i64 f32 f64 externref i32)
                                   (local $ifPredicate i32)
           try (result i32 i64 f32 f64 externref i32)
             ;; Wrong values
             (i32.const 5)
             (i64.const 6)
             (f32.const 0.1)
             (f64.const 0.6437)
             (local.get $wrongRef)
             ${wrongV128}
             ;; throwEmptyExn
             (call_indirect (type $indirectFunctype) (i32.const 1))
             drop ;; Drop the last v128 value.
             (i32.const 0)
           catch_all
              try (result i32 i64 f32 f64 externref ${v128Type})
                 ;; Values to throw.
                 (i32.const 2)
                 (i64.const 3)
                 (f32.const 4)
                 (f64.const 13.37)
                 (local.get $correctRef)
                 ${correctV128}
                 (call_indirect (type $indirectFunctype) (i32.const 2)) ;; returnArgs
                 (call_indirect (type $indirectFunctype) (i32.const 0)) ;; throwExn
                 drop drop ;; Drop v128 and externref to do trivial and irrelevant ops.
                 (f64.const 5)
                 (f64.add)
                 (local.get $wrongRef)
                 ${wrongV128}
                 ;; throwEmptyExn
                 (call_indirect (type $indirectFunctype) (i32.const 1))
                 unreachable
              catch $emptyExn
                  ;; Wrong values
                  (i32.const 5)
                  (i64.const 6)
                  (f32.const 0.1)
                  (f64.const 0.6437)
                  (local.get $wrongRef)
                  ${wrongV128}
              catch $exn
              catch_all
                 ;; Wrong values
                 (i32.const 5)
                 (i64.const 6)
                 (f32.const 0.1)
                 (f64.const 0.6437)
                 (local.get $wrongRef)
                 ${wrongV128}
              end
              ${checkV128Value}
            end))`;

  let testAllValtypes = wasmEvalText(mod, { m : exports}).exports.testFunc;
  assertEqArray(testAllValtypes("foo", "bar"),
                [2, 3n, 4, 13.37, "foo", 1]);
}
