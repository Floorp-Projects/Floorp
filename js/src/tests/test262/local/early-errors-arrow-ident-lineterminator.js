// |reftest| error:ReferenceError
// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-AsyncArrowFunction
description: async arrows cannot have a line terminator between "async" and the AsyncArrowBindingIdentifier
negative:
  phase: early
  type: ReferenceError
---*/

async
foo => { }
