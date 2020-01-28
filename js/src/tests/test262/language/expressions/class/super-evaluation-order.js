// Copyright (C) 2020 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: prod-SuperCall
description: SuperCall should evaluate Arguments prior to checking IsConstructable
info: |
  GetSuperConstructor ( )

  [...]
  1. Let _func_ be ! GetSuperConstructor().
  2. Let _argList_ be ? ArgumentListEvaluation of |Arguments|.
  3. If IsConstructor(_func_) is *false*, throw a *TypeError* exception.
  [...]
features: [default-parameters]
---*/

var x = 0;
class C extends Object {
  constructor() {
    super(x = 123);
  }
}

Object.setPrototypeOf(C, parseInt);

assert.throws(TypeError, () => {
  new C();
});
assert.sameValue(x, 123, 'via ArgumentListEvaluation');

reportCompare(0, 0);
