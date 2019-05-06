// |reftest| skip-if(release_or_beta) error:SyntaxError -- numeric-separator-literal is not released yet
// Copyright (C) 2017 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-NumericLiteralSeparator
description: >
  NumericLiteralSeparator may not appear adjacent to another
  NumericLiteralSeparator in a BinaryIntegerLiteral
info: |
  NumericLiteralSeparator ::
    _

  BinaryIntegerLiteral ::
    0b BinaryDigits
    0B BinaryDigits

  BinaryDigits ::
    BinaryDigit
    BinaryDigits BinaryDigit
    BinaryDigits NumericLiteralSeparator BinaryDigit

  BinaryDigit :: one of
    0 1

negative:
  phase: parse
  type: SyntaxError

features: [numeric-separator-literal]
---*/

$DONOTEVALUATE();

0b0__0
