// |reftest| error:SyntaxError

/*---
author: Jeff Walden <jwalden+code@mit.edu>
esid: sec-let-and-const-declarations
description: >
  Outside AsyncFunction, |await| is a perfectly cromulent LexicalDeclaration
  variable name.  Therefore ASI doesn't apply, and so the |0| where a |=| was
  expected is a syntax error.
---*/

function f() {
    let
    await 0;
}
