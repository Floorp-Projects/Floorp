// |reftest| skip module -- top-level-await is not supported
// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Valid syntax for top level await.
  Nested AwaitExpressions
info: |
  ModuleItem:
    StatementListItem[~Yield, +Await, ~Return]

  ...

  TryStatement[Yield, Await, Return]:
    try Block[?Yield, ?Await, ?Return] Catch[?Yield, ?Await, ?Return]
    try Block[?Yield, ?Await, ?Return] Finally[?Yield, ?Await, ?Return]
    try Block[?Yield, ?Await, ?Return] Catch[?Yield, ?Await, ?Return] Finally[?Yield, ?Await, ?Return]

  ...

  ExpressionStatement[Yield, Await]:
    [lookahead ∉ { {, function, async [no LineTerminator here] function, class, let [ }]
      Expression[+In, ?Yield, ?Await];

  UnaryExpression[Yield, Await]
    [+Await]AwaitExpression[?Yield]

  AwaitExpression[Yield]:
    await UnaryExpression[?Yield, +Await]
esid: prod-AwaitExpression
flags: [module]
features: [top-level-await]
---*/

await await await await await await await await await await await await await await await 'await';

reportCompare(0, 0);
