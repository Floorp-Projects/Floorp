// Copyright (c) 2018 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-update-expressions-static-semantics-early-errors
description: >
  It is an early Reference Error if AssignmentTargetType of LeftHandSideExpression is invalid. (arguments)
info: |

  sec-update-expressions-static-semantics-assignmenttargettype

    UpdateExpression : LeftHandSideExpression ++

    Return invalid.

  sec-update-expressions-static-semantics-early-errors

    UpdateExpression : LeftHandSideExpression ++

    It is an early Reference Error if AssignmentTargetType of LeftHandSideExpression is invalid.

flags: [noStrict]
negative:
  phase: parse
  type: ReferenceError
---*/

$DONOTEVALUATE();

arguments++;

reportCompare(0, 0);
