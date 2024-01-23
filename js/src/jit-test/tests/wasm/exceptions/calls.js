// -----------------------------------------------------------------------------
// This file contains tests checking Wasm functions with throwing functions and
// try-catch code involving more complex control flow, testing that multiple
// values returned from calls in try code are not affected by multiple branching
// towards the landing pad, as well as making sure exceptions carrying multiple
// values of any Wasm numtype transport the exception values correctly across
// calls.
//
// There are tests for local direct calls, for imported direct calls, for
// indirect calls in a local table with local functions, for indirect calls in a
// local table of imported functions, and for indirect calls in an imported
// table of imported functions.
//
// - TODO: Add reftype values, when support for reftypes in exceptions is
//   implemented.
// -----------------------------------------------------------------------------

load(libdir + "eqArrayHelper.js");

// All individual tests take a string 'localThrow' as argument, and will be run
// with each element of the result of the following function.

function generateLocalThrows(types, baseThrow) {
  // Arguments:
  // - 'types': A string of space separated Wasm types.
  // - 'baseThrow': A string with a Wasm instruction sequence of Wasm functype
  //                `[${types}]-> [t*], which takes `types` arguments and ends
  //                up throwing the tag '$exn'.
  // Result:
  // - A JS array of strings, each representing a Wasm instruction sequence
  //   which is like `baseThrow', i.e., a Wasm instruction sequence of Wasm
  //   functype `[${types}]-> [t*], which takes `types` arguments and ends up
  //   throwing the tag '$exn'. The result does not include 'baseThrow'.
  //
  // All strings in Wasm text format.

  // Basic throws;
  let catchlessTryThrow =
      `try (param ${types})
         ${baseThrow}
       end`;

  let catchlessThrowExnref =
      `try_table (param ${types})
         ${baseThrow}
       end`;

  let catchAndThrow =
      `try (param ${types})
         ${baseThrow}
       catch $exn
         ${baseThrow}
       catch_all
       end`;

  let catchAndThrowExnref =
      `(block $join (param ${types})
          (block $catch (param ${types}) (result ${types})
            (block $catchAll (param ${types})
              try_table (param ${types}) (catch $exn $catch) (catch_all $catchAll)
                ${baseThrow}
                unreachable
              end
            )
            br $join
          )
          ${baseThrow}
       )`;

  let blockThrow =
      `(block (param ${types})
         ${baseThrow})`;

  // This Wasm code requires that the function it appears in has an i32 local
  // with name "$ifPredicate".
  let conditionalThrow =
      `(if (param ${types})
         (local.get $ifPredicate)
         (then ${baseThrow})
         (else ${baseThrow}))`;

  // Including try-delegate.
  let baseDelegate =
      `try (param ${types})
        ${baseThrow}
       delegate 0`;

  // Delegate just outside the block.
  let nestedDelegate1InBlock =
      `(block $label1 (param ${types})
         try (param ${types})
           ${baseThrow}
         delegate $label1)`;

  let basicThrows = [catchlessTryThrow, blockThrow, conditionalThrow,
                     baseDelegate, nestedDelegate1InBlock];
  if (wasmExnRefEnabled()) {
    basicThrows = basicThrows.concat(catchlessThrowExnref, catchAndThrowExnref);
  }

  // Secondary throws (will end up inside a catch block).

  let baseRethrow =
      `(rethrow 0)`;

  let nestedRethrow =
      `try (param ${types})
         ${baseThrow}
       catch $exn
         (rethrow 1)
       catch_all
         (rethrow 0)
       end`;

  let catchAllRethrowOriginal =
      `try (param ${types})
         ${baseThrow}
       catch_all
         (rethrow 1)
       end`;

  let secondaryThrows =
      [].concat(basicThrows,
                [baseRethrow, nestedRethrow, catchAllRethrowOriginal]);

  // Nestings.

  function basicNesting (basicThrow, secondaryThrow) {
    return `try (param ${types})
              ${basicThrow}
            catch $exn
              ${secondaryThrow}
            catch_all
            end`;
  };

  let result = [];

  for (let basicThrow of basicThrows) {
    result.push(basicThrow);
    let isExnref = basicThrow == catchlessThrowExnref || basicThrow == catchAndThrowExnref;
    for (let secondaryThrow of secondaryThrows) {
      let isRethrow = secondaryThrow == baseRethrow || secondaryThrow == nestedRethrow || secondaryThrow == catchAllRethrowOriginal;
      if (isExnref && isRethrow) {
        continue;
      }
      result.push(basicNesting(basicThrow, secondaryThrow));
    }
  }

  return result;
};

{
  // Some variables to be used in all tests.
  let typesJS = ["i32", "i64", "f32", "f64", "externref"];
  let types = typesJS.join(" ");

  // The following depend on whether simd is enabled or not. We write it like
  // this so we can run this test also when SIMD is not enabled.
  let exntype = "";
  let wrongV128 = "";
  let throwV128 = "";
  let checkV128Value = "";

  if (wasmSimdEnabled()) {
    exntype = types + " v128";
    wrongV128 = `(v128.const i32x4 11 22 33 44)`;
    throwV128 = `(v128.const i32x4 55 66 77 88)`;
    checkV128Value = `;; Check the V128 value
             ${throwV128}
             (i32x4.eq)
             (i32x4.all_true)`;
  } else {
    exntype = types + " i32";
    wrongV128 = "(i32.const 0)";
    throwV128 = "(i32.const 1)";
    checkV128Value = "";
  }

  let exnTypeDef = `(type $exnType (func (param ${exntype})))`;

  let throwValues =
      `;; Values to throw.
               (i32.const 2)
               (i64.const 3)
               (f32.const 4)
               (f64.const 13.37)
               (local.get $correctRef)
               ${throwV128}`;

  // The last 1 is the result of the test that the v128 value is correct, done
  // in wasm code (if simd is enabled).
  let correctResultsJS = [2, 3n, 4, 13.37, "foo", 1];

  let wrongValues =
      `;; Wrong values
                    (i32.const 5)
                    (i64.const 6)
                    (f32.const 0.1)
                    (f64.const 0.6437)
                    (local.get $wrongRef)
                    ${wrongV128}`;

  // The individual tests. -----------------------------------------------------

  function testDirectCallsThrowing(localThrow) {
    // Test direct function calls throwing any numeric value.

    let throwifTypeInline =
        // The result of the "throwif" function will be used as an argument the
        // second time "throwif" is called.
        `(param $ifPredicate i32) (param $correctRef externref) (result i32)`;

    let moduleHeaderThrowif =
        `(module
           ${exnTypeDef}
           (tag $exn (export "exn") (type $exnType))
           (func $throwif (export "throwif") ${throwifTypeInline}
             (if
               (local.get $ifPredicate)
               (then
                 ${throwValues}
                 ${localThrow}))
             (i32.const 1))`;

    let testModuleRest =
        `(tag $notThrownExn)
         (func $doNothing)
         (func (export "testFunc") (param $correctRef externref)
                                   (param $wrongRef externref)
                                   (result ${types} i32)
                                   (local $ifPredicate i32)
           (local.get $ifPredicate)
           try (param i32) (result ${exntype})
             (local.get $wrongRef)
             (call $throwif) ;; Returns 1.
             (call $doNothing) ;; Does nothing.
             (local.get $correctRef)
             (call $throwif) ;; Throws $exn.
             (drop)
             ${wrongValues} ;; Won't reach this point.
             ${localThrow}
             unreachable
           catch $notThrownExn
             ${wrongValues}
           catch $exn
           end
           ${checkV128Value}))`;

    function testDirectLocalCallsThrowing() {
      let mod = moduleHeaderThrowif + testModuleRest;
      // console.log("DIRECT LOCAL MOD = " + mod);  // Uncomment for debugging.

      assertEqArray(wasmEvalText(mod).exports.testFunc("foo", "bar"),
                    correctResultsJS);
    };

    function testDirectImportedCallsThrowing() {
      let exports = wasmEvalText(moduleHeaderThrowif + `)`).exports;
      // Uncomment for debugging.
      //console.log("DIRECT EXPORTS = " + moduleHeaderThrowif + ")");

      let mod =
          `(module
             ${exnTypeDef}
             (import "m" "exn" (tag $exn (type $exnType)))
             (import "m" "throwif" (func $throwif ${throwifTypeInline}))` +
          testModuleRest;
      // console.log("DIRECT IMPORT MOD = " + mod); // Uncomment for debugging.

      assertEqArray(
        wasmEvalText(mod, { m : exports}).exports.testFunc("foo", "bar"),
        correctResultsJS);
    };

    testDirectLocalCallsThrowing();
    testDirectImportedCallsThrowing();
  };

  function testIndirectCallsThrowing(localThrow) {
    // Test indirect calls throwing exceptions.

    let indirectFunctypeInline = `(param ${exntype})
                                  (result ${exntype})`;
    let getIndirectArgs = `(local.get 0) ;; i32
             (local.get 1) ;; i64
             (local.get 2) ;; f32
             (local.get 3) ;; f64
             (local.get 4) ;; ref
             ;; v128
             (local.get 5)`;

    let testFunctypeInline = `(param $correctRef externref)
                                     (param $wrongRef externref)
                                     ;; The last i32 result is the v128 check.
                                     (result ${types} i32)`;

    let moduleHeader =
        `(module
           ${exnTypeDef}
           (type $indirectFunctype (func ${indirectFunctypeInline}))
           (tag $exn (export "exn") (type $exnType))
           (tag $emptyExn (export "emptyExn"))
           (func $throwExn (export "throwExn") ${indirectFunctypeInline}
                                               (local $ifPredicate i32)
             ${getIndirectArgs}
             ${localThrow}
             unreachable)
           (func $throwEmptyExn (export "throwEmptyExn")
                                ${indirectFunctypeInline}
             (throw $emptyExn)
             unreachable)
           (func $returnArgs (export "returnArgs") ${indirectFunctypeInline}
             ${getIndirectArgs})
           (table (export "tab") funcref (elem $throwExn       ;; 0
                                               $throwEmptyExn  ;; 1
                                               $returnArgs))   ;; 2
           `;

    // The main test function happens to have the same Wasm functype as the
    // indirect calls.
    let testFuncHeader = `(func (export "testFunc") ${testFunctypeInline}
                                     (local $ifPredicate i32)
             `;

    // To test indirect calls to a local table of local functions
    function moduleIndirectLocalLocal(functionBody) {
      return moduleHeader + testFuncHeader + functionBody + `))`;
    };

    let exports = wasmEvalText(moduleHeader + ")").exports;
    // Uncomment for debugging.
    //console.log("INDIRECT EXPORTS = " + moduleHeader + ")");

    let moduleHeaderImporting =
        `(module
           ${exnTypeDef}
           (type $indirectFunctype (func ${indirectFunctypeInline}))
           (import "m" "exn" (tag $exn (type $exnType)))
           (import "m" "emptyExn" (tag $emptyExn))
           (import "m" "throwExn" (func $throwExn (type $indirectFunctype)))
           (import "m" "throwEmptyExn"
                   (func $throwEmptyExn (type $indirectFunctype)))
           (import "m" "returnArgs"
                   (func $returnArgs (type $indirectFunctype)))`;

    // To test indirect calls to a local table of imported functions.
    function moduleIndirectLocalImport(functionBody) {
      return moduleHeaderImporting +
        `(table funcref (elem $throwExn $throwEmptyExn $returnArgs))` +
        testFuncHeader + functionBody + `))`;
    };

    // To test indirect calls to an imported table of imported functions.
    function moduleIndirectImportImport(functionBody) {
      return moduleHeaderImporting +
        `(import "m" "tab" (table 3 funcref))` +
        testFuncHeader + functionBody + `))`;
    };

    function getModuleTextsForFunctionBody(functionBody) {
      return [moduleIndirectLocalLocal(functionBody),
              moduleIndirectLocalImport(functionBody),
              moduleIndirectImportImport(functionBody)];
    };

    // The function bodies for the tests.

    // Three indirect calls, the middle of which throws and will be caught. The
    // results of the first and second indirect calls are used by the next
    // indirect call. This should be called from try code, to check that the
    // pad-branches don't interfere with the results of each call.
    let indirectThrow = `${throwValues}
               (call_indirect (type $indirectFunctype) (i32.const 2)) ;; returnArgs
               (call_indirect (type $indirectFunctype) (i32.const 0)) ;; throwExn
               drop drop ;; Drop v128 and externref to do trivial and irrelevant ops.
               (f64.const 5)
               (f64.add)
               (local.get $wrongRef)
               ${wrongV128}
               ;; throwEmptyExn
               (call_indirect (type $indirectFunctype) (i32.const 1))
               unreachable`;

    // Simple try indirect throw and catch.
    let simpleTryIndirect = `
            try (result ${exntype})
              ${indirectThrow}
              catch $emptyExn
                 ${wrongValues}
              catch $exn
              catch_all
                 ${wrongValues}
            end
            ${checkV128Value}`;

    // Indirect throw/catch_all, with a simple try indirect throw nested in the
    // catch_all.
    let nestedTryIndirect =
        `try (result ${types} i32)
           ${wrongValues}
           ;; throwEmptyExn
           (call_indirect (type $indirectFunctype) (i32.const 1))
           drop ;; Drop the last v128 value.
           (i32.const 0)
         catch_all
            ${simpleTryIndirect}
         end`;

    let functionBodies = [simpleTryIndirect, nestedTryIndirect];

    // Test throwing from indirect calls.
    for (let functionBody of functionBodies) {
      // console.log("functionBody = : " + functionBody); // Uncomment for debugging.

      for (let mod of getModuleTextsForFunctionBody(functionBody)) {
        //console.log("mod = : " + mod); // Uncomment for debugging.

        let testFunction = wasmEvalText(mod, { m : exports}).exports.testFunc;
        assertEqArray(testFunction("foo", "bar"),
                      correctResultsJS);
      }
    }
  };

  // Run all tests. ------------------------------------------------------------

  let localThrows =
      ["(throw $exn)"].concat(generateLocalThrows(exntype, "(throw $exn)"));

  for (let localThrow of localThrows) {
    // Uncomment for debugging.
    // console.log("Testing with localThrow = " + localThrow);

    testDirectCallsThrowing(localThrow);
    testIndirectCallsThrowing(localThrow);
  }
}
