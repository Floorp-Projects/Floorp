// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaintimeiso
description: Functions when time zone argument is omitted
features: [Temporal]
---*/

const t = Temporal.Now.plainTimeISO();
assert(t instanceof Temporal.PlainTime);

reportCompare(0, 0);
