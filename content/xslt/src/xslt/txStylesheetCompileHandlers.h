/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXSTYLESHEETCOMPILEHANDLERS_H
#define TRANSFRMX_TXSTYLESHEETCOMPILEHANDLERS_H

#include "txError.h"
#include "txNamespaceMap.h"
#include "txExpandedNameMap.h"

struct txStylesheetAttr;
class txStylesheetCompilerState;

typedef nsresult (*HandleStartFn) (PRInt32 aNamespaceID,
                                   nsIAtom* aLocalName,
                                   nsIAtom* aPrefix,
                                   txStylesheetAttr* aAttributes,
                                   PRInt32 aAttrCount,
                                   txStylesheetCompilerState& aState);
typedef nsresult (*HandleEndFn)   (txStylesheetCompilerState& aState);
typedef nsresult (*HandleTextFn)  (const nsAString& aStr,
                                   txStylesheetCompilerState& aState);

struct txElementHandler {
    PRInt32 mNamespaceID;
    const char* mLocalName;
    HandleStartFn mStartFunction;
    HandleEndFn mEndFunction;
};

class txHandlerTable
{
public:
    txHandlerTable(const HandleTextFn aTextHandler,
                   const txElementHandler* aLREHandler,
                   const txElementHandler* aOtherHandler);
    nsresult init(const txElementHandler* aHandlers, PRUint32 aCount);
    const txElementHandler* find(PRInt32 aNamespaceID, nsIAtom* aLocalName);
    
    const HandleTextFn mTextHandler;
    const txElementHandler* const mLREHandler;

    static bool init();
    static void shutdown();

private:
    const txElementHandler* const mOtherHandler;
    txExpandedNameMap<const txElementHandler> mHandlers;
};

extern txHandlerTable* gTxRootHandler;
extern txHandlerTable* gTxEmbedHandler;

#endif //TRANSFRMX_TXSTYLESHEETCOMPILEHANDLERS_H
