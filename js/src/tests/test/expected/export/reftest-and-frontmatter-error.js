// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.


/*---
esid: sec-let-and-const-declarations
features: []
negative:
  phase: runtime
  type: SyntaxError
description: |
  Outside AsyncFunction, |await| is a perfectly cromulent LexicalDeclaration variable
  name.  Therefore ASI doesn't apply, and so the |0| where a |=| was expected is a
  syntax error.
---*/

eval(`
    function f() {
        let
        await 0;
    }
`);
