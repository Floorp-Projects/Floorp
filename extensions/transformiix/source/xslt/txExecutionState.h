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

#ifndef TRANSFRMX_TXEXECUTIONSTATE_H
#define TRANSFRMX_TXEXECUTIONSTATE_H

#include "txCore.h"
#include "txStack.h"
#include "XMLUtils.h"
#include "nsVoidArray.h"
#include "txIXPathContext.h"
#include "txVariableMap.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "txKey.h"
#include "txStylesheet.h"

class txAOutputHandlerFactory;
class txAXMLEventHandler;
class txInstruction;
class txIOutputHandlerFactory;
class txExpandedNameMap;

class txLoadedDocumentEntry : public nsStringHashKey
{
public:
    txLoadedDocumentEntry(KeyTypePointer aStr) : nsStringHashKey(aStr)
    {
    }
    txLoadedDocumentEntry(const txLoadedDocumentEntry& aToCopy)
        : nsStringHashKey(aToCopy)
    {
        NS_ERROR("We're horked.");
    }
    ~txLoadedDocumentEntry()
    {
        if (mDocument) {
            txXPathNodeUtils::release(mDocument);
        }
    }

    nsAutoPtr<txXPathNode> mDocument;
};

class txLoadedDocumentsHash : public nsTHashtable<txLoadedDocumentEntry>
{
public:
    ~txLoadedDocumentsHash();
    nsresult init(txXPathNode* aSourceDocument);

private:
    friend class txExecutionState;
    txXPathNode* mSourceDocument;
};


class txExecutionState : public txIMatchContext
{
public:
    txExecutionState(txStylesheet* aStylesheet);
    ~txExecutionState();
    nsresult init(const txXPathNode& aNode, txExpandedNameMap* aGlobalParams);
    nsresult end();

    TX_DECL_MATCH_CONTEXT;

    /**
     * Struct holding information about a current template rule
     */
    struct TemplateRule {
        txStylesheet::ImportFrame* mFrame;
        PRInt32 mModeNsId;
        nsIAtom* mModeLocalName;
        txVariableMap* mParams;
    };

    // Stack functions
    nsresult pushEvalContext(txIEvalContext* aContext);
    txIEvalContext* popEvalContext();
    nsresult pushString(const nsAString& aStr);
    void popString(nsAString& aStr);
    nsresult pushInt(PRInt32 aInt);
    PRInt32 popInt();
    nsresult pushResultHandler(txAXMLEventHandler* aHandler);
    txAXMLEventHandler* popResultHandler();
    nsresult pushTemplateRule(txStylesheet::ImportFrame* aFrame,
                              const txExpandedName& aMode,
                              txVariableMap* aParams);
    void popTemplateRule();
    nsresult pushParamMap(txVariableMap* aParams);
    txVariableMap* popParamMap();

    // state-getting functions
    txIEvalContext* getEvalContext();
    txExpandedNameMap* getParamMap();
    const txXPathNode* retrieveDocument(const nsAString& aUri);
    nsresult getKeyNodes(const txExpandedName& aKeyName,
                         const txXPathNode& aDocument,
                         const nsAString& aKeyValue, PRBool aIndexIfNotFound,
                         txNodeSet** aResult);
    TemplateRule* getCurrentTemplateRule();

    // state-modification functions
    txInstruction* getNextInstruction();
    nsresult runTemplate(txInstruction* aInstruction);
    nsresult runTemplate(txInstruction* aInstruction,
                         txInstruction* aReturnTo);
    void gotoInstruction(txInstruction* aNext);
    void returnFromTemplate();
    nsresult bindVariable(const txExpandedName& aName,
                          txAExprResult* aValue);
    void removeVariable(const txExpandedName& aName);

    txAXMLEventHandler* mOutputHandler;
    txAXMLEventHandler* mResultHandler;
    txAOutputHandlerFactory* mOutputHandlerFactory;

    nsAutoPtr<txVariableMap> mTemplateParams;

    nsRefPtr<txStylesheet> mStylesheet;

private:
    txStack mReturnStack;
    txStack mLocalVarsStack;
    txStack mEvalContextStack;
    txStack mIntStack;
    txStack mResultHandlerStack;
    txStack mParamStack;
    nsStringArray mStringStack;
    txInstruction* mNextInstruction;
    txVariableMap* mLocalVariables;
    txVariableMap mGlobalVariableValues;
    nsRefPtr<txAExprResult> mGlobalVarPlaceholderValue;
    PRInt32 mRecursionDepth;

    TemplateRule* mTemplateRules;
    PRInt32 mTemplateRulesBufferSize;
    PRInt32 mTemplateRuleCount;

    txIEvalContext* mEvalContext;
    txIEvalContext* mInitialEvalContext;
    //Document* mRTFDocument;
    txExpandedNameMap* mGlobalParams;

    txLoadedDocumentsHash mLoadedDocuments;
    txKeyHash mKeyHash;
    nsRefPtr<txResultRecycler> mRecycler;

    static const PRInt32 kMaxRecursionDepth;
};

#endif
