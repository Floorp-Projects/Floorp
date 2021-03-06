// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "\"{}\" Block within a \"while\" Expression is evaluated to true"
es5id: 12.6.2_A11
description: Checking if execution of "while({}){}" passes
---*/

while({}){
    var __in__do=1;
    if(__in__do)break;
};

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__in__do !== 1) {
	throw new Test262Error('#1: "{}" in while expression evaluates to true');
}
//
//////////////////////////////////////////////////////////////////////////////

reportCompare(0, 0);
