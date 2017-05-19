// |reftest| skip-if(release_or_beta) error:SyntaxError -- async-iteration is not released yet
// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-labelled-statements
description: >
  AsyncGeneratorDeclaration is not allowed in statement position
info: |
  ExpressionStatement[Yield, Await] :
    [lookahead ∉ { {, function, async [no LineTerminator here] function, class, let [ }]
    Expression[+In, ?Yield, ?Await] ;
negative:
  phase: early
  type: SyntaxError
features: [async-iteration]
---*/

label: async function* g() {}
