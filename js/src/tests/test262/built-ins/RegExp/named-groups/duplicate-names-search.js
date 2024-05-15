// |reftest| shell-option(--enable-regexp-duplicate-named-groups) skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&!getBuildConfiguration('release_or_beta'))||!xulRuntime.shell) -- regexp-duplicate-named-groups is not enabled unconditionally, requires shell-options
// Copyright 2022 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: String.prototype.search behavior with duplicate named capture groups
esid: prod-GroupSpecifier
features: [regexp-duplicate-named-groups]
---*/

assert.sameValue("xab".search(/(?<x>a)|(?<x>b)/), 1);
assert.sameValue("xba".search(/(?<x>a)|(?<x>b)/), 1);

reportCompare(0, 0);
