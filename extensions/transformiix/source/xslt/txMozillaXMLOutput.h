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
 * The Original Code is the TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
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

#ifndef TRANSFRMX_MOZILLA_XML_OUTPUT_H
#define TRANSFRMX_MOZILLA_XML_OUTPUT_H

#include "txXMLEventHandler.h"
#include "nsIContent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIScriptLoader.h"
#include "nsIScriptLoaderObserver.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsWeakPtr.h"
#include "txOutputFormat.h"
#include "nsCOMArray.h"
#include "nsICSSLoaderObserver.h"
#include "nsIDocumentTransformer.h"

#define TX_ITRANSFORMNOTIFIER_IID \
{ 0x6c94c701, 0x330d, 0x11d7, \
  { 0xa7, 0xf2, 0x9a, 0x44, 0x5a, 0xec, 0x64, 0x3c } };

class txITransformNotifier : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(TX_ITRANSFORMNOTIFIER_IID)

    NS_IMETHOD_(void) AddScriptElement(nsIDOMHTMLScriptElement* aElement) = 0;
    NS_IMETHOD_(void) AddStyleSheet(nsIStyleSheet* aStyleSheet) = 0;
    NS_IMETHOD_(void) OnTransformEnd() = 0;
    NS_IMETHOD_(void) OnTransformStart() = 0;
    NS_IMETHOD_(void) SetOutputDocument(nsIDOMDocument* aDocument) = 0;
};

class txMozillaXMLOutput : public txAOutputXMLEventHandler
{
public:
    txMozillaXMLOutput(const nsAString& aRootName,
                       PRInt32 aRootNsID,
                       txOutputFormat* aFormat,
                       nsIDOMDocument* aSourceDocument,
                       nsIDOMDocument* aResultDocument,
                       nsITransformObserver* aObserver);
    txMozillaXMLOutput(txOutputFormat* aFormat,
                       nsIDOMDocumentFragment* aFragment);
    virtual ~txMozillaXMLOutput();

    TX_DECL_TXAXMLEVENTHANDLER
    TX_DECL_TXAOUTPUTXMLEVENTHANDLER

private:
    void closePrevious(PRInt8 aAction);
    void startHTMLElement(nsIDOMElement* aElement);
    void endHTMLElement(nsIDOMElement* aElement, PRBool aXHTML);
    void processHTTPEquiv(nsIAtom* aHeader, const nsAString& aValue);
    void wrapChildren(nsIDOMNode* aCurrentNode, nsIDOMElement* aWrapper);
    nsresult createResultDocument(const nsAString& aName, PRInt32 aNsID,
                                  nsIDOMDocument* aSourceDocument,
                                  nsIDOMDocument* aResultDocument);

    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsIDOMNode> mCurrentNode;
    nsCOMPtr<nsIDOMNode> mParentNode;
    nsCOMPtr<nsIContent> mRootContent;

    nsCOMPtr<nsIDOMNode> mNonAddedParent;
    nsCOMPtr<nsIDOMNode> mNonAddedNode;

    nsCOMPtr<txITransformNotifier> mNotifier;

    PRUint32 mBadChildLevel;
    nsCString mRefreshString;

    nsAutoString mText;

    txOutputFormat mOutputFormat;

    PRPackedBool mDontAddCurrent;

    PRPackedBool mHaveTitleElement;
    PRPackedBool mHaveBaseElement;

    PRPackedBool mDocumentIsHTML;

    enum txAction { eCloseElement = 1, eFlushText = 2 };
};

class txTransformNotifier : public txITransformNotifier,
                            public nsIScriptLoaderObserver,
                            public nsICSSLoaderObserver
{
public:
    txTransformNotifier(nsITransformObserver* aObserver);
    virtual ~txTransformNotifier();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTLOADEROBSERVER
    
    // nsICSSLoaderObserver
    NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aNotify);

    NS_IMETHOD_(void) AddScriptElement(nsIDOMHTMLScriptElement* aElement);
    NS_IMETHOD_(void) AddStyleSheet(nsIStyleSheet* aStyleSheet);
    NS_IMETHOD_(void) OnTransformEnd();
    NS_IMETHOD_(void) OnTransformStart();
    NS_IMETHOD_(void) SetOutputDocument(nsIDOMDocument* aDocument);

private:
    void SignalTransformEnd();

    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsITransformObserver> mObserver;
    nsCOMArray<nsIDOMHTMLScriptElement> mScriptElements;
    nsCOMArray<nsIStyleSheet> mStylesheets;
    PRPackedBool mInTransform;
};

#endif
