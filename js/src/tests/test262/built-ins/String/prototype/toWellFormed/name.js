// |reftest| shell-option(--enable-well-formed-unicode-strings) skip-if(!String.prototype.toWellFormed||!xulRuntime.shell) -- String.prototype.toWellFormed is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.towellformed
description: >
  String.prototype.toWellFormed.name value and descriptor.
info: |
  String.prototype.toWellFormed( )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
features: [String.prototype.toWellFormed]
---*/

verifyProperty(String.prototype.toWellFormed, 'name', {
  enumerable: false,
  writable: false,
  configurable: true,
  value: 'toWellFormed'
});

reportCompare(0, 0);
