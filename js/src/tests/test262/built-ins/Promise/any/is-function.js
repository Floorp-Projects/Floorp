// |reftest| skip-if(release_or_beta) -- Promise.any is not released yet
// Copyright (C) 2019 Sergey Rubanov. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-promise.any
description: Promise.any is callable
features: [Promise.any]
---*/

assert.sameValue(typeof Promise.any, 'function');

reportCompare(0, 0);
