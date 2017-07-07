/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXTOPLEVELITEMS_H
#define TRANSFRMX_TXTOPLEVELITEMS_H

#include "nsError.h"
#include "txOutputFormat.h"
#include "txXMLUtils.h"
#include "txStylesheet.h"
#include "txInstructions.h"

class txPattern;
class Expr;

class txToplevelItem
{
public:
    txToplevelItem()
    {
        MOZ_COUNT_CTOR(txToplevelItem);
    }
    virtual ~txToplevelItem()
    {
        MOZ_COUNT_DTOR(txToplevelItem);
    }

    enum type {
        attributeSet,
        dummy,
        import,
        //namespaceAlias,
        output,
        stripSpace, //also used for preserve-space
        templ,
        variable
    };

    virtual type getType() = 0;
};

#define TX_DECL_TOPLEVELITEM virtual type getType();
#define TX_IMPL_GETTYPE(_class, _type) \
txToplevelItem::type \
_class::getType() { return _type;}

class txInstructionContainer : public txToplevelItem
{
public:
    nsAutoPtr<txInstruction> mFirstInstruction;
};

// xsl:attribute-set
class txAttributeSetItem : public txInstructionContainer
{
public:
    explicit txAttributeSetItem(const txExpandedName aName) : mName(aName)
    {
    }

    TX_DECL_TOPLEVELITEM

    txExpandedName mName;
};

// xsl:import
class txImportItem : public txToplevelItem
{
public:
    TX_DECL_TOPLEVELITEM

    nsAutoPtr<txStylesheet::ImportFrame> mFrame;
};

// xsl:output
class txOutputItem : public txToplevelItem
{
public:
    TX_DECL_TOPLEVELITEM

    txOutputFormat mFormat;
};

// insertionpoint for xsl:include
class txDummyItem : public txToplevelItem
{
public:
    TX_DECL_TOPLEVELITEM
};

// xsl:strip-space and xsl:preserve-space
class txStripSpaceItem : public txToplevelItem
{
public:
    ~txStripSpaceItem();

    TX_DECL_TOPLEVELITEM

    nsresult addStripSpaceTest(txStripSpaceTest* aStripSpaceTest);

    nsTArray<txStripSpaceTest*> mStripSpaceTests;
};

// xsl:template
class txTemplateItem : public txInstructionContainer
{
public:
    txTemplateItem(nsAutoPtr<txPattern>&& aMatch, const txExpandedName& aName,
                   const txExpandedName& aMode, double aPrio);

    TX_DECL_TOPLEVELITEM

    nsAutoPtr<txPattern> mMatch;
    txExpandedName mName;
    txExpandedName mMode;
    double mPrio;
};

// xsl:variable at top level
class txVariableItem : public txInstructionContainer
{
public:
    txVariableItem(const txExpandedName& aName, nsAutoPtr<Expr>&& aValue,
                   bool aIsParam);

    TX_DECL_TOPLEVELITEM

    txExpandedName mName;
    nsAutoPtr<Expr> mValue;
    bool mIsParam;
};

#endif //TRANSFRMX_TXTOPLEVELITEMS_H
