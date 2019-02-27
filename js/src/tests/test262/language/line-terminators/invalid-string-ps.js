// |reftest| error:SyntaxError
// Copyright 2019 Mike Pennisi.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: PARAGRAPH SEPARATOR (U+2029) within strings is not allowed
es5id: 7.3_A2.2_T1
esid: sec-line-terminators
description: Insert PARAGRAPH SEPARATOR (\u2029) into string
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

' '
