// |reftest| skip-if(release_or_beta) -- Intl.DateTimeFormat-formatRange is not released yet
// Copyright 2019 Google, Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Intl.DateTimeFormat.prototype.formatRange.length.
includes: [propertyHelper.js]
features: [Intl.DateTimeFormat-formatRange]
---*/
verifyProperty(Intl.DateTimeFormat.prototype.formatRange, 'length', {
  value: 2,
  enumerable: false,
  writable: false,
  configurable: true,
});

reportCompare(0, 0);
