// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Jeff Walden <jwalden+code@mit.edu>
description: |
  '|await| is excluded from LexicalDeclaration by grammar parameter, in AsyncFunction.  Therefore
  |let| followed by |await| inside AsyncFunction is an ASI opportunity, and this code
  must parse without error.'
esid: sec-let-and-const-declarations
---*/

async function f() {
    let
    await 0;
}

assert.sameValue(true, f instanceof Function);
