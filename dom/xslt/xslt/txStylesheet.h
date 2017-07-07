/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TX_TXSTYLESHEET_H
#define TX_TXSTYLESHEET_H

#include "txOutputFormat.h"
#include "txExpandedNameMap.h"
#include "txList.h"
#include "txXSLTPatterns.h"
#include "nsISupportsImpl.h"

class txInstruction;
class txTemplateItem;
class txVariableItem;
class txStripSpaceItem;
class txAttributeSetItem;
class txDecimalFormat;
class txStripSpaceTest;
class txXSLKey;

class txStylesheet final
{
public:
    class ImportFrame;
    class GlobalVariable;
    friend class txStylesheetCompilerState;
    // To be able to do some cleaning up in destructor
    friend class ImportFrame;

    txStylesheet();
    nsresult init();

    NS_INLINE_DECL_REFCOUNTING(txStylesheet)

    nsresult findTemplate(const txXPathNode& aNode,
                          const txExpandedName& aMode,
                          txIMatchContext* aContext,
                          ImportFrame* aImportedBy,
                          txInstruction** aTemplate,
                          ImportFrame** aImportFrame);
    txDecimalFormat* getDecimalFormat(const txExpandedName& aName);
    txInstruction* getAttributeSet(const txExpandedName& aName);
    txInstruction* getNamedTemplate(const txExpandedName& aName);
    txOutputFormat* getOutputFormat();
    GlobalVariable* getGlobalVariable(const txExpandedName& aName);
    const txOwningExpandedNameMap<txXSLKey>& getKeyMap();
    nsresult isStripSpaceAllowed(const txXPathNode& aNode,
                                 txIMatchContext* aContext,
                                 bool& aAllowed);

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
                              nsAutoPtr<txDecimalFormat>&& aFormat);

    struct MatchableTemplate {
        txInstruction* mFirstInstruction;
        nsAutoPtr<txPattern> mMatch;
        double mPriority;
    };

    /**
     * Contain information that is import precedence dependant.
     */
    class ImportFrame {
    public:
        ImportFrame()
            : mFirstNotImported(nullptr)
        {
        }
        ~ImportFrame();

        // List of toplevel items
        txList mToplevelItems;

        // Map of template modes
        txOwningExpandedNameMap< nsTArray<MatchableTemplate> > mMatchableTemplates;

        // ImportFrame which is the first one *not* imported by this frame
        ImportFrame* mFirstNotImported;
    };

    class GlobalVariable : public txObject {
    public:
        GlobalVariable(nsAutoPtr<Expr>&& aExpr,
                       nsAutoPtr<txInstruction>&& aFirstInstruction,
                       bool aIsParam);

        nsAutoPtr<Expr> mExpr;
        nsAutoPtr<txInstruction> mFirstInstruction;
        bool mIsParam;
    };

private:
    // Private destructor, to discourage deletion outside of Release():
    ~txStylesheet();

    nsresult addTemplate(txTemplateItem* aTemplate, ImportFrame* aImportFrame);
    nsresult addGlobalVariable(txVariableItem* aVariable);
    nsresult addFrames(txListIterator& aInsertIter);
    nsresult addStripSpace(txStripSpaceItem* aStripSpaceItem,
                           nsTArray<txStripSpaceTest*>& aFrameStripSpaceTests);
    nsresult addAttributeSet(txAttributeSetItem* aAttributeSetItem);

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
    txExpandedNameMap<txInstruction> mNamedTemplates;

    // Map with all decimal-formats
    txOwningExpandedNameMap<txDecimalFormat> mDecimalFormats;

    // Map with all named attribute sets
    txExpandedNameMap<txInstruction> mAttributeSets;

    // Map with all global variables and parameters
    txOwningExpandedNameMap<GlobalVariable> mGlobalVariables;

    // Map with all keys
    txOwningExpandedNameMap<txXSLKey> mKeys;

    // Array of all txStripSpaceTests, sorted in acending order
    nsTArray<nsAutoPtr<txStripSpaceTest> > mStripSpaceTests;

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
    txStripSpaceTest(nsIAtom* aPrefix, nsIAtom* aLocalName, int32_t aNSID,
                     bool stripSpace)
        : mNameTest(aPrefix, aLocalName, aNSID, txXPathNodeType::ELEMENT_NODE),
          mStrips(stripSpace)
    {
    }

    nsresult matches(const txXPathNode& aNode, txIMatchContext* aContext,
                     bool& aMatched)
    {
        return mNameTest.matches(aNode, aContext, aMatched);
    }

    bool stripsSpace() {
        return mStrips;
    }

    double getDefaultPriority() {
        return mNameTest.getDefaultPriority();
    }

protected:
    txNameTest mNameTest;
    bool mStrips;
};

/**
 * Value of a global parameter
 */
class txIGlobalParameter
{
public:
    txIGlobalParameter()
    {
        MOZ_COUNT_CTOR(txIGlobalParameter);
    }
    virtual ~txIGlobalParameter()
    {
        MOZ_COUNT_DTOR(txIGlobalParameter);
    }
    virtual nsresult getValue(txAExprResult** aValue) = 0;
};


#endif //TX_TXSTYLESHEET_H
