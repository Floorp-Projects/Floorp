// |reftest| shell-option(--enable-regexp-duplicate-named-groups) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&!getBuildConfiguration('release_or_beta'))||!xulRuntime.shell) -- regexp-duplicate-named-groups is not enabled unconditionally, requires shell-options
// Copyright 2022 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: String.prototype.replace behavior with duplicate named capture groups
esid: prod-GroupSpecifier
features: [regexp-duplicate-named-groups]
---*/

assert.sameValue("ab".replace(/(?<x>a)|(?<x>b)/, "[$<x>]"), "[a]b");
assert.sameValue("ba".replace(/(?<x>a)|(?<x>b)/, "[$<x>]"), "[b]a");

assert.sameValue("ab".replace(/(?<x>a)|(?<x>b)/, "[$<x>][$1][$2]"), "[a][a][]b");
assert.sameValue("ba".replace(/(?<x>a)|(?<x>b)/, "[$<x>][$1][$2]"), "[b][][b]a");

assert.sameValue("ab".replace(/(?<x>a)|(?<x>b)/g, "[$<x>]"), "[a][b]");
assert.sameValue("ba".replace(/(?<x>a)|(?<x>b)/g, "[$<x>]"), "[b][a]");

reportCompare(0, 0);
