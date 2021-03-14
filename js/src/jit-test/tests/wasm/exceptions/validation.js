// Test wasm type validation for exception handling instructions.

load(libdir + "wasm-binary.js");

function wasmValid(mod) {
  assertEq(WebAssembly.validate(mod), true);
}

function wasmInvalid(mod, pattern) {
  assertEq(WebAssembly.validate(mod), false);
  assertErrorMessage(
    () => new WebAssembly.Module(mod),
    WebAssembly.CompileError,
    pattern
  );
}

const emptyType = { args: [], ret: VoidCode };
const i32Type = { args: [I32Code], ret: VoidCode };
const toi32Type = { args: [], ret: I32Code };
const i32Toi32Type = { args: [I32Code], ret: I32Code };
const i32Toi64Type = { args: [I32Code], ret: I64Code };
const i32i32Toi32Type = { args: [I32Code, I32Code], ret: I32Code };

function testValidateDecode() {
  // Try blocks must have a block type code.
  wasmInvalid(
    moduleWithSections([
      sigSection([emptyType]),
      declSection([0]),
      eventSection([{ type: 0 }]),
      bodySection([
        funcBody({
          locals: [],
          body: [
            TryCode,
            // Missing type code.
            I32ConstCode,
            0x01,
            CatchCode,
            0x00,
            EndCode,
            DropCode,
            ReturnCode,
          ],
        }),
      ]),
    ]),
    /bad type/
  );

  // Catch must have an event index.
  wasmInvalid(
    moduleWithSections([
      sigSection([emptyType]),
      declSection([0]),
      eventSection([{ type: 0 }]),
      bodySection([
        funcBody(
          {
            locals: [],
            body: [
              TryCode,
              I32Code,
              I32ConstCode,
              0x01,
              CatchCode,
              // Index missing.
            ],
          },
          (withEndCode = false)
        ),
      ]),
    ]),
    /expected event index/
  );

  // Try blocks must have a second branch after the main body.
  wasmInvalid(
    moduleWithSections([
      sigSection([emptyType]),
      declSection([0]),
      eventSection([{ type: 0 }]),
      bodySection([
        funcBody({
          locals: [],
          body: [
            TryCode,
            I32Code,
            I32ConstCode,
            0x01,
            // Missing instruction here.
            EndCode,
            DropCode,
            ReturnCode,
          ],
        }),
      ]),
    ]),
    /try without catch or unwind not allowed/
  );
}

function testValidateThrow() {
  valid = `(module
             (type (func (param i32)))
             (func $exn-zero
               i32.const 0
               throw $exn1)
             (event $exn1 (type 0)))`;

  invalid0 = `(module
                (type (func (param i32)))
                (func $exn-zero
                  throw $exn1)
                (event $exn1 (type 0)))`;
  error0 = /popping value from empty stack/;

  invalid1 = `(module
                (type (func (param i32)))
                (func $exn-zero
                  i64.const 0
                  throw $exn1)
                (event $exn1 (type 0)))`;
  error1 = /expression has type i64 but expected i32/;

  invalid2 = `(module
                (type (func (param i32)))
                (func $exn-zero
                  i32.const 0
                  throw 1)
                (event $exn1 (type 0)))`;
  error2 = /event index out of range/;

  wasmValidateText(valid);
  wasmFailValidateText(invalid0, error0);
  wasmFailValidateText(invalid1, error1);
  wasmFailValidateText(invalid2, error2);
}

function testValidateTryCatch() {
  function mod_with(fbody) {
    return moduleWithSections([
      sigSection([emptyType, i32Type, i32i32Toi32Type]),
      declSection([0]),
      eventSection([{ type: 0 }, { type: 1 }]),
      bodySection([
        funcBody({
          locals: [],
          body: fbody,
        }),
      ]),
    ]);
  }

  const body1 = [
    // try (result i32)
    TryCode,
    I32Code,
    // (i32.const 1)
    I32ConstCode,
    varU32(1),
    // catch 1
    CatchCode,
    varU32(1),
  ];

  const valid1 = mod_with(body1.concat([EndCode, DropCode, ReturnCode]));
  const invalid1 = mod_with(
    body1.concat([I32ConstCode, varU32(2), EndCode, DropCode, ReturnCode])
  );

  const valid2 = mod_with([
    // (i32.const 0) (i32.const 0)
    I32ConstCode,
    varU32(0),
    I32ConstCode,
    varU32(0),
    // try (param i32 i32) (result i32) drop drop (i32.const 1)
    TryCode,
    varS32(2),
    DropCode,
    DropCode,
    I32ConstCode,
    varU32(1),
    // catch 0 (i32.const 2) end drop return
    CatchCode,
    varU32(0),
    I32ConstCode,
    varU32(2),
    EndCode,
    DropCode,
    ReturnCode,
  ]);

  wasmValid(valid1);
  wasmInvalid(invalid1, /unused values not explicitly dropped/);
  wasmValid(valid2);
}

function testValidateCatch() {
  wasmInvalid(
    moduleWithSections([
      sigSection([emptyType]),
      declSection([0]),
      bodySection([
        funcBody({
          locals: [],
          body: [TryCode, VoidCode, CatchCode, varU32(0), EndCode],
        }),
      ]),
    ]),
    /event index out of range/
  );
}

function testValidateExnPayload() {
  valid0 = moduleWithSections([
    sigSection([i32Type, i32Toi32Type]),
    declSection([1]),
    // (event $exn (param i32))
    eventSection([{ type: 0 }]),
    bodySection([
      // (func (param i32) (result i32) ...
      funcBody({
        locals: [],
        body: [
          // try (result i32) (local.get 0) (throw $exn) (i32.const 1)
          TryCode,
          I32Code,
          LocalGetCode,
          varU32(0),
          ThrowCode,
          varU32(0),
          I32ConstCode,
          varU32(1),
          // catch $exn (i32.const 1) (i32.add) end
          CatchCode,
          varU32(0),
          I32ConstCode,
          varU32(1),
          I32AddCode,
          EndCode,
        ],
      }),
    ]),
  ]);

  // This is to ensure the following sentence from the spec overview holds:
  // > "the operand stack is popped back to the size the operand stack had
  // > when the try block was entered"
  valid1 = moduleWithSections([
    sigSection([i32Type, toi32Type]),
    declSection([1]),
    // (event $exn (param i32))
    eventSection([{ type: 0 }]),
    bodySection([
      // (func (result i32) ...
      funcBody({
        locals: [],
        body: [
          // try (result i32) (i32.const 0) (i32.const 1) (throw $exn) drop
          TryCode,
          I32Code,
          I32ConstCode,
          varU32(0),
          I32ConstCode,
          varU32(1),
          ThrowCode,
          varU32(0),
          DropCode,
          // catch $exn drop (i32.const 2) end
          CatchCode,
          varU32(0),
          DropCode,
          I32ConstCode,
          varU32(2),
          EndCode,
        ],
      }),
    ]),
  ]);

  invalid0 = moduleWithSections([
    sigSection([i32Type, i32Toi64Type]),
    declSection([1]),
    // (event $exn (param i32))
    eventSection([{ type: 0 }]),
    bodySection([
      // (func (param i32) (result i64) ...
      funcBody({
        locals: [],
        body: [
          // try (result i64) (local.get 0) (throw $exn) (i64.const 0)
          TryCode,
          I64Code,
          LocalGetCode,
          varU32(0),
          ThrowCode,
          varU32(0),
          I64ConstCode,
          varU32(0),
          // catch $exn end
          CatchCode,
          varU32(0),
          EndCode,
        ],
      }),
    ]),
  ]);

  invalid1 = moduleWithSections([
    // (type (func))
    sigSection([emptyType]),
    declSection([0]),
    // (event $exn (type 0))
    eventSection([{ type: 0 }]),
    bodySection([
      // (func ...
      funcBody({
        locals: [],
        body: [
          // try catch 1 end
          TryCode,
          VoidCode,
          CatchCode,
          varU32(1),
          EndCode,
        ],
      }),
    ]),
  ]);

  wasmValid(valid0);
  wasmValid(valid1);
  wasmInvalid(invalid0, /has type i32 but expected i64/);
  wasmInvalid(invalid1, /event index out of range/);
}

testValidateDecode();
testValidateThrow();
testValidateTryCatch();
testValidateCatch();
testValidateExnPayload();
