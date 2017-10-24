// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-typeof-operator-runtime-semantics-evaluation
description: typeof of BigInt and BigInt object
info: >
  The typeof Operator

  Runtime Semantics: Evaluation
features: [BigInt]
---*/

assert.sameValue(typeof 0n, "bigint");
assert.sameValue(typeof Object(0n), "object");

reportCompare(0, 0);
