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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * Jonas Sicking. All Rights Reserved.
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

#ifndef TX_TXSTYLESHEET_H
#define TX_TXSTYLESHEET_H

#include "txOutputFormat.h"
#include "txExpandedNameMap.h"
#include "List.h"
#include "Expr.h"
#include "txXSLTPatterns.h"
#include "txInstructions.h"
#include "XSLTFunctions.h"

class txInstruction;
class txToplevelItem;
class txTemplateItem;
class txVariableItem;
class txStripSpaceItem;
class txAttributeSetItem;

class txStylesheet
{
public:
    class ImportFrame;
    class GlobalVariable;
    friend class txStylesheetCompilerState;
    // To be able to do some cleaning up in destructor
    friend class ImportFrame;

    txStylesheet();
    ~txStylesheet();
    nsresult init();

    nsrefcnt AddRef()
    {
        return ++mRefCnt;
    }
    nsrefcnt Release()
    {
        if (--mRefCnt == 0) {
            mRefCnt = 1; //stabilize
            delete this;
            return 0;
        }
        return mRefCnt;
    }

    txInstruction* findTemplate(Node* aNode,
                                const txExpandedName& aMode,
                                txIMatchContext* aContext,
                                ImportFrame* aImportedBy,
                                ImportFrame** aImportFrame);
    txDecimalFormat* getDecimalFormat(const txExpandedName& aName);
    txInstruction* getAttributeSet(const txExpandedName& aName);
    txInstruction* getNamedTemplate(const txExpandedName& aName);
    txOutputFormat* getOutputFormat();
    GlobalVariable* getGlobalVariable(const txExpandedName& aName);
    const txExpandedNameMap& getKeyMap();
    PRBool isStripSpaceAllowed(Node* aNode, txIMatchContext* aContext);

    /**
     * Called by the stylesheet compiler once all stylesheets has been read.
     */
    nsresult doneCompiling();

    /**
     * Add a key to the stylesheet
     */
    nsresult addKey(const txExpandedName& aName, nsAutoPtr<txPattern> aMatch,
                    nsAutoPtr<Expr> aUse);

    /**
     * Add a decimal-format to the stylesheet
     */
    nsresult addDecimalFormat(const txExpandedName& aName,
                              nsAutoPtr<txDecimalFormat> aFormat);

    /**
     * Contain information that is import precedence dependant.
     */
    class ImportFrame {
    public:
        ImportFrame();
        ~ImportFrame();

        // List of toplevel items
        txList mToplevelItems;

        // Map of template modes, each item in the map is a txList
        // of templates
        txExpandedNameMap mMatchableTemplates;

        // ImportFrame which is the first one *not* imported by this frame
        ImportFrame* mFirstNotImported;
    };

    class GlobalVariable : public TxObject {
    public:
        GlobalVariable(nsAutoPtr<Expr> aExpr,
                       nsAutoPtr<txInstruction> aFirstInstruction,
                       PRBool aIsParam);

        nsAutoPtr<Expr> mExpr;
        nsAutoPtr<txInstruction> mFirstInstruction;
        PRBool mIsParam;
    };

private:
    class MatchableTemplate {
    public:
        MatchableTemplate(txInstruction* aFirstInstruction,
                          nsAutoPtr<txPattern> aPattern,
                          double aPriority)
            : mFirstInstruction(aFirstInstruction),
              mMatch(aPattern),
              mPriority(aPriority)
        {
        }
        txInstruction* mFirstInstruction;
        nsAutoPtr<txPattern> mMatch;
        double mPriority;
    };
    
    nsresult addTemplate(txTemplateItem* aTemplate, ImportFrame* aImportFrame);
    nsresult addGlobalVariable(txVariableItem* aVariable);
    nsresult addFrames(txListIterator& aInsertIter);
    nsresult addStripSpace(txStripSpaceItem* aStripSpaceItem,
                           nsVoidArray& frameStripSpaceTests);
    nsresult addAttributeSet(txAttributeSetItem* aAttributeSetItem);

    // Refcount
    nsAutoRefCnt mRefCnt;

    // List of ImportFrames
    txList mImportFrames;
    
    // output format
    txOutputFormat mOutputFormat;

    // List of first instructions of templates. This is the owner of all
    // instructions used in templates
    txList mTemplateInstructions;
    
    // Root importframe
    ImportFrame* mRootFrame;
    
    // Named templates
    txExpandedNameMap mNamedTemplates;
    
    // Map with all decimal-formats
    txExpandedNameMap mDecimalFormats;

    // Map with all named attribute sets
    txExpandedNameMap mAttributeSets;
    
    // Map with all global variables and parameters
    txExpandedNameMap mGlobalVariables;
    
    // Map with all keys
    txExpandedNameMap mKeys;
    
    // Array of all txStripSpaceTests, sorted in acending order
    nsVoidArray mStripSpaceTests;
    
    // Default templates
    nsAutoPtr<txInstruction> mContainerTemplate;
    nsAutoPtr<txInstruction> mCharactersTemplate;
    nsAutoPtr<txInstruction> mEmptyTemplate;
};


/**
 * txStripSpaceTest holds both an txNameTest and a bool for use in
 * whitespace stripping.
 */
class txStripSpaceTest {
public:
    txStripSpaceTest(nsIAtom* aPrefix, nsIAtom* aLocalName, PRInt32 aNSID,
                     MBool stripSpace)
        : mNameTest(aPrefix, aLocalName, aNSID, Node::ELEMENT_NODE),
          mStrips(stripSpace)
    {
    }

    MBool matches(Node* aNode, txIMatchContext* aContext) {
        return mNameTest.matches(aNode, aContext);
    }

    MBool stripsSpace() {
        return mStrips;
    }

    double getDefaultPriority() {
        return mNameTest.getDefaultPriority();
    }

protected:
    txNameTest mNameTest;
    MBool mStrips;
};

/**
 * Value of a global parameter
 */
class txIGlobalParameter : public TxObject
{
public:
    virtual nsresult getValue(ExprResult** aValue) = 0;
};


#endif //TX_TXSTYLESHEET_H
