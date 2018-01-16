// Copyright 2015 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: |
 Symbol.species is retained on subclassing
author: Sam Mikes
description: Symbol.species is retained on subclassing
includes:
  - propertyHelper.js
features: [Symbol.species]
---*/

class MyRegExp extends RegExp {
};

assert.sameValue(MyRegExp[Symbol.species], MyRegExp);


reportCompare(0, 0);
