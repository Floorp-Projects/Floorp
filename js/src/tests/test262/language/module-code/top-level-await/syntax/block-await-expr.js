// |reftest| skip module -- top-level-await is not supported
// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Valid syntax for top level await. It propagates to the StatementList of a Block
info: |
  ModuleItem:
    StatementListItem[~Yield, +Await, ~Return]

  StatementListItem[Yield, Await, Return]:
    Statement[?Yield, ?Await, ?Return]
    Declaration[?Yield, ?Await]

  Statement[Yield, Await, Return]:
    BlockStatement[?Yield, ?Await, ?Return]

  BlockStatement[Yield, Await, Return]:
    Block[?Yield, ?Await, ?Return]

  Block[Yield, Await, Return]:
    { StatementList[?Yield, ?Await, ?Return]_opt }

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

{
  {
    {
      {
        {
          {
            {
              {
                {
                  {
                    {
                      {
                        {
                          {
                            {
                              {
                                {
                                  await {};
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

reportCompare(0, 0);
