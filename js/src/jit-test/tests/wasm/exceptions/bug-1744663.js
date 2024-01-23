gczeal(2,1);

function failureCallingTestFunction() {
  let exports = wasmEvalText(
    `(module
       (tag $exn (export "exn"))
       (func $throwExn (export "throwExn")
         ;; Note that this does not fail if this function body is a plain (throw $exn).
         (try
           (do (throw $exn)))))`
  ).exports;

  let mod =
      `(module
         (type $exnType (func))
         (type $indirectFunctype (func))
         (import "m" "exn" (tag $exn (type $exnType)))
         (import "m" "throwExn" (func $throwExn (type $indirectFunctype)))
         (table funcref (elem $throwExn))
         (func (export "testFunc") (result i32)
           (try
             (do (call_indirect (type $indirectFunctype) (i32.const 0)))
             (catch_all))
           (i32.const 1)))`;

  let testFunction = wasmEvalText(mod, { m : exports}).exports.testFunc;
  testFunction();
};

function failureRethrow1() {
  let exports = wasmEvalText(
    `(module
       (tag $exn (export "exn"))
       (func $throwExn (export "throwExn")
         (try
           (do (throw $exn))
           (catch_all
             (try
               (do (throw $exn))
               (catch_all (rethrow 1)))))))`
  ).exports;

  let mod =
      `(module
         (type $exnType (func))
         (type $indirectFunctype (func))
         (import "m" "exn" (tag $exn (type $exnType)))
         (import "m" "throwExn" (func $throwExn (type $indirectFunctype)))
         (table funcref (elem $throwExn))
         (func (export "testFunc") (result i32)
           (try
             (do (call_indirect (type $indirectFunctype) (i32.const 0)))
             (catch_all))
           (i32.const 1)))`;

  let testFunction = wasmEvalText(mod, { m : exports}).exports.testFunc;
  testFunction();
};

console.log("Calling failureCallingTestFunction.");
failureCallingTestFunction();
console.log("Calling failureRethrow1.");
failureRethrow1();
