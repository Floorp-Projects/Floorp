// Ensure memory operations work after a throw coming from a function call.
// These are also testing that the InstanceReg/HeapReg are set correctly after
// catching an exception. There are some variations to the kind of calls here;
// we test for direct/indirect calls of local/imported functions, and the
// indirect calls may come from a local table or an imported table.

// Throw from a direct/indirect call of a local/imported function.
{
  let exports = wasmEvalText(
    `(module $m
       (memory $mem (data "bar"))
       (tag $exn (export "exn"))
       (tag $dummyExn (export "dummyExn"))
       (func $throwsExn (export "throwsExn")
         (throw $exn))
       (func $anotherThrowsExn
         (throw $exn))
       (func $throwsDummyExn (export "throwsDummyExn")
         (throw $dummyExn))
       (table (export "tab") funcref (elem $anotherThrowsExn $throwsDummyExn)))`
  ).exports;

  function testMemoryAfterCall(callInstruction) {
    assertEq(
      wasmEvalText(
        `(module
           (import "m" "exn" (tag $exn))
           (tag $localExn (param i32))
           (type $t (func))
           (import "m" "tab" (table $importTable 2 funcref))
           (import "m" "throwsExn" (func $importFuncThrowsExn))
           (memory $mem (data "foo"))
           (func $localFuncThrowsExn
             (throw $exn))
           (table $localTable funcref
                  (elem $localFuncThrowsExn $importFuncThrowsExn))
           (func $anotherLocalFuncThrowsExn
             (throw $exn))
           (func $throwsLocalExn
             (throw $localExn
               (i32.const 9)))
           (func (export "testFunc") (result i32)
             try (result i32)
               ${callInstruction}
               ;; All the rest should not be reachable.
               (call $anotherLocalFuncThrowsExn)
               (throw $exn)
               (call $throwsLocalExn)
               unreachable
             catch $localExn
             catch $exn
               (i32.load8_u
                 (i32.const 0))
             catch $exn
               ;; This should be ignored.
               unreachable
             end))`,
        { m: exports }
      ).exports.testFunc(),
      'foo'.charCodeAt(0)
    );
  };

  // Run test for various calls.
  let callInstructions =
      ["(call $anotherLocalFuncThrowsExn)",
       "(call $importFuncThrowsExn)",
       // Calls $localFuncThrowsExn.
       "(call_indirect $localTable (type $t) (i32.const 0))",
       // Calls $importFuncThrowsExn.
       "(call_indirect $localTable (type $t) (i32.const 1))",
       // Calls non exported function of the exports module $anotherThrowsExn.
       "(call_indirect $importTable (type $t) (i32.const 0))"];

  for (let callInstruction of callInstructions) {
    testMemoryAfterCall(callInstruction);
  }
}
