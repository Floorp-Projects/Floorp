/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef TRANSFRMX_TXTOPLEVELITEMS_H
#define TRANSFRMX_TXTOPLEVELITEMS_H

#include "txError.h"
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
    txAttributeSetItem(const txExpandedName aName) : mName(aName)
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
    txTemplateItem(nsAutoPtr<txPattern> aMatch, const txExpandedName& aName,
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
    txVariableItem(const txExpandedName& aName, nsAutoPtr<Expr> aValue,
                   bool aIsParam);
    
    TX_DECL_TOPLEVELITEM

    txExpandedName mName;
    nsAutoPtr<Expr> mValue;
    bool mIsParam;
};

#endif //TRANSFRMX_TXTOPLEVELITEMS_H
