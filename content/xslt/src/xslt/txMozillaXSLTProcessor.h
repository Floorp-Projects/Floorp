/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXMOZILLAXSLTPROCESSOR_H
#define TRANSFRMX_TXMOZILLAXSLTPROCESSOR_H

#include "nsAutoPtr.h"
#include "nsStubMutationObserver.h"
#include "nsIDocumentTransformer.h"
#include "nsIXSLTProcessor.h"
#include "nsIXSLTProcessorPrivate.h"
#include "txExpandedNameMap.h"
#include "txNamespaceMap.h"
#include "nsIJSNativeInitializer.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

class nsIDOMNode;
class nsIPrincipal;
class nsIURI;
class nsIXMLContentSink;
class txStylesheet;
class txResultRecycler;
class txIGlobalParameter;

/* bacd8ad0-552f-11d3-a9f7-000064657374 */
#define TRANSFORMIIX_XSLT_PROCESSOR_CID   \
{ 0x618ee71d, 0xd7a7, 0x41a1, {0xa3, 0xfb, 0xc2, 0xbe, 0xdc, 0x6a, 0x21, 0x7e} }

#define TRANSFORMIIX_XSLT_PROCESSOR_CONTRACTID \
"@mozilla.org/document-transformer;1?type=xslt"

#define XSLT_MSGS_URL  "chrome://global/locale/xslt/xslt.properties"

/**
 * txMozillaXSLTProcessor is a front-end to the XSLT Processor.
 */
class txMozillaXSLTProcessor MOZ_FINAL : public nsIXSLTProcessor,
                                         public nsIXSLTProcessorPrivate,
                                         public nsIDocumentTransformer,
                                         public nsStubMutationObserver,
                                         public nsIJSNativeInitializer
{
public:
    /**
     * Creates a new txMozillaXSLTProcessor
     */
    txMozillaXSLTProcessor();

    /**
     * Default destructor for txMozillaXSLTProcessor
     */
    ~txMozillaXSLTProcessor();

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(txMozillaXSLTProcessor,
                                             nsIXSLTProcessor)

    // nsIXSLTProcessor interface
    NS_DECL_NSIXSLTPROCESSOR

    // nsIXSLTProcessorPrivate interface
    NS_DECL_NSIXSLTPROCESSORPRIVATE

    // nsIDocumentTransformer interface
    NS_IMETHOD Init(nsIPrincipal* aPrincipal);
    NS_IMETHOD SetTransformObserver(nsITransformObserver* aObserver);
    NS_IMETHOD LoadStyleSheet(nsIURI* aUri, nsILoadGroup* aLoadGroup);
    NS_IMETHOD SetSourceContentModel(nsIDOMNode* aSource);
    NS_IMETHOD CancelLoads() {return NS_OK;}
    NS_IMETHOD AddXSLTParamNamespace(const nsString& aPrefix,
                                     const nsString& aNamespace);
    NS_IMETHOD AddXSLTParam(const nsString& aName,
                            const nsString& aNamespace,
                            const nsString& aSelect,
                            const nsString& aValue,
                            nsIDOMNode* aContext);

    // nsIMutationObserver interface
    NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

    nsresult setStylesheet(txStylesheet* aStylesheet);
    void reportError(nsresult aResult, const PRUnichar *aErrorText,
                     const PRUnichar *aSourceText);

    nsIDOMNode *GetSourceContentModel()
    {
        return mSource;
    }

    nsresult TransformToDoc(nsIDOMDocument **aResult,
                            bool aCreateDataDocument);

    bool IsLoadDisabled()
    {
        return (mFlags & DISABLE_ALL_LOADS) != 0;
    }

    // nsIJSNativeInitializer
    NS_IMETHODIMP Initialize(nsISupports* aOwner, JSContext *cx, JSObject *obj,
                             const JS::CallArgs& aArgs);

    static nsresult Startup();
    static void Shutdown();

private:
    nsresult DoTransform();
    void notifyError();
    nsresult ensureStylesheet();

    nsRefPtr<txStylesheet> mStylesheet;
    nsIDocument* mStylesheetDocument; // weak
    nsCOMPtr<nsIContent> mEmbeddedStylesheetRoot;

    nsCOMPtr<nsIDOMNode> mSource;
    nsresult mTransformResult;
    nsresult mCompileResult;
    nsString mErrorText, mSourceText;
    nsCOMPtr<nsIPrincipal> mPrincipal;
    nsCOMPtr<nsITransformObserver> mObserver;
    txOwningExpandedNameMap<txIGlobalParameter> mVariables;
    txNamespaceMap mParamNamespaceMap;
    nsRefPtr<txResultRecycler> mRecycler;

    uint32_t mFlags;
};

extern nsresult TX_LoadSheet(nsIURI* aUri, txMozillaXSLTProcessor* aProcessor,
                             nsILoadGroup* aLoadGroup,
                             nsIPrincipal* aCallerPrincipal);

extern nsresult TX_CompileStylesheet(nsINode* aNode,
                                     txMozillaXSLTProcessor* aProcessor,
                                     nsIPrincipal* aCallerPrincipal,
                                     txStylesheet** aStylesheet);

#endif
