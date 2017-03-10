// |reftest| error:SyntaxError
// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-AsyncArrowHead
description: async arrows cannot have a line terminator between "async" and the formals
negative:
  phase: early
  type: SyntaxError
---*/

async
(foo,bar) => { }
