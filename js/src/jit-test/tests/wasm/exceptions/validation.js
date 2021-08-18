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
      tagSection([{ type: 0 }]),
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

  // Catch must have a tag index.
  wasmInvalid(
    moduleWithSections([
      sigSection([emptyType]),
      declSection([0]),
      tagSection([{ type: 0 }]),
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
    /expected tag index/
  );

  // Rethrow must have a depth argument.
  wasmInvalid(
    moduleWithSections([
      sigSection([emptyType]),
      declSection([0]),
      tagSection([{ type: 0 }]),
      bodySection([
        funcBody(
          {
            locals: [],
            body: [
              RethrowCode,
              // Index missing.
            ],
          },
          (withEndCode = false)
        ),
      ]),
    ]),
    /unable to read rethrow depth/
  );

  // Delegate must have a depth argument.
  wasmInvalid(
    moduleWithSections([
      sigSection([emptyType]),
      declSection([0]),
      tagSection([{ type: 0 }]),
      bodySection([
        funcBody(
          {
            locals: [],
            body: [
              TryCode,
              I32Code,
              I32ConstCode,
              0x01,
              DelegateCode,
              // Index missing.
            ],
          },
          (withEndCode = false)
        ),
      ]),
    ]),
    /unable to read delegate depth/
  );
}

function testValidateThrow() {
  valid = `(module
             (type (func (param i32)))
             (func $exn-zero
               i32.const 0
               throw $exn1)
             (tag $exn1 (type 0)))`;

  invalid0 = `(module
                (type (func (param i32)))
                (func $exn-zero
                  throw $exn1)
                (tag $exn1 (type 0)))`;
  error0 = /popping value from empty stack/;

  invalid1 = `(module
                (type (func (param i32)))
                (func $exn-zero
                  i64.const 0
                  throw $exn1)
                (tag $exn1 (type 0)))`;
  error1 = /expression has type i64 but expected i32/;

  invalid2 = `(module
                (type (func (param i32)))
                (func $exn-zero
                  i32.const 0
                  throw 1)
                (tag $exn1 (type 0)))`;
  error2 = /tag index out of range/;

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
      tagSection([{ type: 0 }, { type: 1 }]),
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

  // Test handler-less try blocks.
  wasmValidateText(
    `(module (func try end))`
  );

  wasmValidateText(
    `(module (func (result i32) try (result i32) (i32.const 1) end))`
  );

  wasmValidateText(
    `(module
       (func (result i32)
         try (result i32) (i32.const 1) (br 0) end))`
  );

  wasmFailValidateText(
    `(module
       (func try (result i32) end))`,
    /popping value from empty stack/
  );
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
    /tag index out of range/
  );
}

function testValidateCatchAll() {
  wasmValidateText(
    `(module
       (tag $exn)
       (func try catch $exn catch_all end))`
  );

  wasmValidateText(
    `(module
       (func (result i32)
         try (result i32)
           (i32.const 0)
         catch_all
           (i32.const 1)
         end))`
  );

  wasmFailValidateText(
    `(module
       (tag $exn)
       (func try catch_all catch 0 end))`,
    /catch cannot follow a catch_all/
  );

  wasmFailValidateText(
    `(module
       (tag $exn)
       (func try (result i32) (i32.const 1) catch_all end drop))`,
    /popping value from empty stack/
  );

  wasmFailValidateText(
    `(module
       (tag $exn (param i32))
       (func try catch $exn drop catch_all drop end))`,
    /popping value from empty stack/
  );

  // We can't distinguish `else` and `catch_all` in error messages since they
  // share the binary opcode.
  wasmFailValidateText(
    `(module
       (tag $exn)
       (func try catch_all catch_all end))`,
    /catch_all can only be used within a try/
  );

  wasmFailValidateText(
    `(module
       (tag $exn)
       (func catch_all))`,
    /catch_all can only be used within a try/
  );
}

function testValidateExnPayload() {
  valid0 = moduleWithSections([
    sigSection([i32Type, i32Toi32Type]),
    declSection([1]),
    // (tag $exn (param i32))
    tagSection([{ type: 0 }]),
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
    // (tag $exn (param i32))
    tagSection([{ type: 0 }]),
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
    // (tag $exn (param i32))
    tagSection([{ type: 0 }]),
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
    // (tag $exn (type 0))
    tagSection([{ type: 0 }]),
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
  wasmInvalid(invalid1, /tag index out of range/);
}

function testValidateRethrow() {
  wasmValidateText(
    `(module
       (tag $exn (param))
       (func
         try
           nop
         catch $exn
           rethrow 0
         end))`
  );

  wasmValidateText(
    `(module
       (tag $exn (param))
       (func
         try
           nop
         catch_all
           rethrow 0
         end))`
  );

  wasmValidateText(
    `(module
       (func (result i32)
         try (result i32)
           (i32.const 1)
         catch_all
           rethrow 0
         end))`
  );

  wasmValidateText(
    `(module
       (tag $exn (param))
       (func
         try
           nop
         catch $exn
           block
             try
             catch $exn
               rethrow 0
             end
           end
         end))`
  );

  wasmValidateText(
    `(module
       (tag $exn (param))
       (func
         try
           nop
         catch $exn
           block
             try
             catch $exn
               rethrow 2
             end
           end
         end))`
  );

  wasmFailValidateText(
    `(module
       (tag $exn (param))
       (func
         try
           nop
         catch $exn
           block
             try
             catch $exn
               rethrow 1
             end
           end
         end))`,
    /rethrow target was not a catch block/
  );

  wasmFailValidateText(
    `(module (func rethrow 0))`,
    /rethrow target was not a catch block/
  );

  wasmFailValidateText(
    `(module (func try rethrow 0 end))`,
    /rethrow target was not a catch block/
  );

  wasmFailValidateText(
    `(module (func try rethrow 0 catch_all end))`,
    /rethrow target was not a catch block/
  );

  wasmFailValidateText(
    `(module
       (tag $exn (param))
       (func
         try
           nop
         catch $exn
           block
             try
             catch $exn
               rethrow 4
             end
           end
         end))`,
    /rethrow depth exceeds current nesting level/
  );
}

function testValidateDelegate() {
  wasmValidateText(
    `(module
       (tag $exn (param))
       (func
         try
           try
             throw $exn
           delegate 0
         catch $exn
         end))`
  );

  wasmValidateText(
    `(module
       (tag $exn (param))
       (func
         try
           try
             throw $exn
           delegate 1
         catch $exn
         end))`
  );

  wasmFailValidateText(
    `(module
       (tag $exn (param))
       (func (result i32)
         try
           throw $exn
         delegate 0
         (i64.const 0)
         end))`,
    /type mismatch: expression has type i64 but expected i32/
  );

  wasmFailValidateText(
    `(module
       (tag $exn (param))
       (func
         try (result i32)
           (i64.const 0)
         delegate 0))`,
    /type mismatch: expression has type i64 but expected i32/
  );

  wasmFailValidateText(
    `(module
       (tag $exn (param))
       (func
         try
           try
             throw $exn
           delegate 2
         catch $exn
         end))`,
    /delegate depth exceeds current nesting level/
  );

  wasmFailValidateText(
    `(module
       (tag $exn (param))
       (func
         block
           try
             throw $exn
           delegate 0
         end))`,
    /delegate target was not a try or function body/
  );

  wasmFailValidateText(
    `(module
       (tag $exn (param))
       (func
         try
         catch $exn
           try
             throw $exn
           delegate 0
         end))`,
    /delegate target was not a try or function body/
  );

  wasmFailValidateText(
    `(module (func delegate 0))`,
    /delegate can only be used within a try/
  );
}

testValidateDecode();
testValidateThrow();
testValidateTryCatch();
testValidateCatch();
testValidateCatchAll();
testValidateExnPayload();
testValidateRethrow();
testValidateDelegate();
