// |reftest| shell-option(--enable-regexp-duplicate-named-groups) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&!getBuildConfiguration('release_or_beta'))||!xulRuntime.shell) -- regexp-duplicate-named-groups is not enabled unconditionally, requires shell-options
// Copyright 2022 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: match indices with duplicate named capture groups
esid: sec-makematchindicesindexpairarray
features: [regexp-duplicate-named-groups, regexp-match-indices]
includes: [compareArray.js]
---*/

let indices = "..ab".match(/(?<x>a)|(?<x>b)/d).indices;
assert.compareArray(indices.groups.x, [2, 3]);

indices = "..ba".match(/(?<x>a)|(?<x>b)/d).indices;
assert.compareArray(indices.groups.x, [2, 3]);

reportCompare(0, 0);
