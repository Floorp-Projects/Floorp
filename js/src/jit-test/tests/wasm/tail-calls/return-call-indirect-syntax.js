// |jit-test| skip-if: !wasmTailCallsEnabled()

function assertInvalidSyntax(module) {
    assertErrorMessage(() => wasmTextToBinary(module), SyntaxError,
                       /wasm text error/);
}

assertInvalidSyntax(`
  (module
    (type $sig (func (param i32) (result i32)))
    (table 0 anyfunc)
    (func (result i32)
      (return_call_indirect (type $sig) (result i32) (param i32)
        (i32.const 0) (i32.const 0)
      )
    )
  )`);

assertInvalidSyntax(`
  (module
    (type $sig (func (param i32 i32) (result i32 i32)))
    (table 0 anyfunc)
    (func (result i32)
      (return_call_indirect (param i32) (type $sig) (result i32)
        (i32.const 0) (i32.const 0)
      )
    )
  )`);

assertInvalidSyntax(`
  (module
    (type $sig (func (param i32) (result i32)))
    (table 0 anyfunc)
    (func (result i32)
      (return_call_indirect (param i32) (result i32) (type $sig)
        (i32.const 0) (i32.const 0)
      )
    )
  )`);

assertInvalidSyntax(`
  (module
    (type $sig (func (param i32) (result i32)))
    (table 0 anyfunc)
    (func (result i32)
      (return_call_indirect (result i32) (type $sig) (param i32)
        (i32.const 0) (i32.const 0)
      )
    )
  )`);

assertInvalidSyntax(`
  (module
    (type $sig (func (param i32) (result i32)))
    (table 0 anyfunc)
    (func (result i32)
      (return_call_indirect (result i32) (param i32) (type $sig)
        (i32.const 0) (i32.const 0)
      )
    )
  )`);

assertInvalidSyntax(`
  (module
    (table 0 anyfunc)
    (func (result i32)
      (return_call_indirect (result i32) (param i32)
        (i32.const 0) (i32.const 0)
      )
    )
  )`);

assertInvalidSyntax(`
  (module
    (table 0 anyfunc)
    (func (return_call_indirect (param $x i32) (i32.const 0) (i32.const 0)))
  )`);

assertInvalidSyntax(`
  (module
    (type $sig (func))
    (table 0 anyfunc)
    (func (result i32)
      (return_call_indirect (type $sig) (result i32) (i32.const 0))
    )
  )`);

assertInvalidSyntax(`
  (module
    (type $sig (func (param i32) (result i32)))
    (table 0 anyfunc)
    (func (result i32)
      (return_call_indirect (type $sig) (result i32) (i32.const 0))
    )
  )`);

assertInvalidSyntax(`
  (module
    (type $sig (func (param i32) (result i32)))
    (table 0 anyfunc)
    (func
      (return_call_indirect (type $sig) (param i32)
        (i32.const 0) (i32.const 0)
      )
    )
  )`);

assertInvalidSyntax(`
  (module
    (type $sig (func (param i32 i32) (result i32)))
    (table 0 anyfunc)
    (func (result i32)
      (return_call_indirect (type $sig) (param i32) (result i32)
        (i32.const 0) (i32.const 0)
      )
    )
  )`);
