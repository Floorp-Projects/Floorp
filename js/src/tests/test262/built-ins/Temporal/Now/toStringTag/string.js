// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-now-@@tostringtag
description: The @@toStringTag property of Temporal.Now produces the correct value in toString
features: [Symbol.toStringTag, Temporal]
---*/

assert.sameValue(String(Temporal.Now), "[object Temporal.Now]");

reportCompare(0, 0);
