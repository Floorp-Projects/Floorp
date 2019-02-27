// Copyright (c) 2018 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-update-expressions
description: >
  It is an early Reference Error if AssignmentTargetType of UnaryExpression is invalid. (arguments)
info: |

  sec-update-expressions-static-semantics-assignmenttargettype

    UpdateExpression : ++ UnaryExpression

    Return invalid.

  sec-update-expressions-static-semantics-early-errors

    UpdateExpression ++ UnaryExpression

    It is an early Reference Error if AssignmentTargetType of UnaryExpression is invalid.

flags: [noStrict]
negative:
  phase: parse
  type: ReferenceError
---*/

$DONOTEVALUATE();

++arguments;

reportCompare(0, 0);
