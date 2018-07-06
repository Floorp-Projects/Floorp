// |reftest| skip -- global is not supported
// Copyright (C) 2016 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-other-properties-of-the-global-object-global
description: "'global' should be writable, non-enumerable, and configurable"
author: Jordan Harband
includes: [propertyHelper.js]
features: [global]
---*/

verifyProperty(this, "global", {
    enumerable: false,
    writable: true,
    configurable: true,
});

reportCompare(0, 0);
