/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TX_PATTERNPARSER_H
#define TX_PATTERNPARSER_H

#include "txXSLTPatterns.h"
#include "txExprParser.h"

class txStylesheetCompilerState;

class txPatternParser : public txExprParser
{
public:
    static nsresult createPattern(const nsAFlatString& aPattern,
                                  txIParseContext* aContext,
                                  txPattern** aResult);
protected:
    static nsresult createUnionPattern(txExprLexer& aLexer,
                                       txIParseContext* aContext,
                                       txPattern*& aPattern);
    static nsresult createLocPathPattern(txExprLexer& aLexer,
                                         txIParseContext* aContext,
                                         txPattern*& aPattern);
    static nsresult createIdPattern(txExprLexer& aLexer,
                                    txPattern*& aPattern);
    static nsresult createKeyPattern(txExprLexer& aLexer,
                                     txIParseContext* aContext,
                                     txPattern*& aPattern);
    static nsresult createStepPattern(txExprLexer& aLexer,
                                      txIParseContext* aContext,
                                      txPattern*& aPattern);
};

#endif // TX_PATTERNPARSER_H
