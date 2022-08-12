// |reftest| shell-option(--enable-array-grouping) skip-if(!Array.prototype.group||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Chengzhong Wu. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype-@@unscopables
description: >
    Initial value of `Symbol.unscopables` property
info: |
    22.1.3.32 Array.prototype [ @@unscopables ]

    ...
    10. Perform ! CreateDataPropertyOrThrow(unscopableList, "group", true).
    11. Perform ! CreateDataPropertyOrThrow(unscopableList, "groupToMap", true).
    ...

includes: [propertyHelper.js]
features: [Symbol.unscopables, array-grouping]
---*/

var unscopables = Array.prototype[Symbol.unscopables];

assert.sameValue(Object.getPrototypeOf(unscopables), null);

assert.sameValue(unscopables.group, true, '`group` property value');
verifyEnumerable(unscopables, 'group');
verifyWritable(unscopables, 'group');
verifyConfigurable(unscopables, 'group');

assert.sameValue(unscopables.groupToMap, true, '`groupToMap` property value');
verifyEnumerable(unscopables, 'groupToMap');
verifyWritable(unscopables, 'groupToMap');
verifyConfigurable(unscopables, 'groupToMap');

reportCompare(0, 0);
