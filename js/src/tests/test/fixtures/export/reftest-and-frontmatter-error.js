// |reftest| error:SyntaxError

/*---
esid: sec-let-and-const-declarations
description: >
  Outside AsyncFunction, |await| is a perfectly cromulent LexicalDeclaration
  variable name.  Therefore ASI doesn't apply, and so the |0| where a |=| was
  expected is a syntax error.
negative:
  phase: runtime
  type: SyntaxError
---*/

eval(`
    function f() {
        let
        await 0;
    }
`);
