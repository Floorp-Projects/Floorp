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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#include "txMozillaXSLTProcessor.h"
#include "nsContentCID.h"
#include "nsDOMError.h"
#include "nsIChannel.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIDocument.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIScriptLoader.h"
#include "nsIStringBundle.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "txExecutionState.h"
#include "txMozillaTextOutput.h"
#include "txMozillaXMLOutput.h"
#include "txURIUtils.h"
#include "XMLUtils.h"
#include "txUnknownHandler.h"
#include "txXSLTProcessor.h"
#include "nsIPrincipal.h"

static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);

/**
 * Output Handler Factories
 */
class txToDocHandlerFactory : public txAOutputHandlerFactory
{
public:
    txToDocHandlerFactory(txExecutionState* aEs,
                          nsIDOMDocument* aSourceDocument,
                          nsIDOMDocument* aResultDocument,
                          nsITransformObserver* aObserver)
        : mEs(aEs), mSourceDocument(aSourceDocument),
          mResultDocument(aResultDocument), mObserver(aObserver)
    {
    }

    virtual ~txToDocHandlerFactory()
    {
    }

    TX_DECL_TXAOUTPUTHANDLERFACTORY

private:
    txExecutionState* mEs;
    nsCOMPtr<nsIDOMDocument> mSourceDocument;
    nsCOMPtr<nsIDOMDocument> mResultDocument;
    nsCOMPtr<nsITransformObserver> mObserver;
};

class txToFragmentHandlerFactory : public txAOutputHandlerFactory
{
public:
    txToFragmentHandlerFactory(nsIDOMDocumentFragment* aFragment)
        : mFragment(aFragment)
    {
    }

    virtual ~txToFragmentHandlerFactory()
    {
    }

    TX_DECL_TXAOUTPUTHANDLERFACTORY

private:
    nsCOMPtr<nsIDOMDocumentFragment> mFragment;
};

nsresult
txToDocHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                         txAXMLEventHandler** aHandler)
{
    *aHandler = nsnull;
    switch (aFormat->mMethod) {
        case eMethodNotSet:
        case eXMLOutput:
        {
            *aHandler = new txUnknownHandler(mEs);
            break;
        }

        case eHTMLOutput:
        {
            *aHandler = new txMozillaXMLOutput(EmptyString(),
                                               kNameSpaceID_None,
                                               aFormat, mSourceDocument,
                                               mResultDocument, mObserver);
            break;
        }

        case eTextOutput:
        {
            *aHandler = new txMozillaTextOutput(mSourceDocument,
                                                mResultDocument,
                                                mObserver);
            break;
        }
    }
    NS_ENSURE_TRUE(*aHandler, NS_ERROR_OUT_OF_MEMORY);
    return NS_OK;
}

nsresult
txToDocHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                         const nsAString& aName,
                                         PRInt32 aNsID,
                                         txAXMLEventHandler** aHandler)
{
    *aHandler = nsnull;
    switch (aFormat->mMethod) {
        case eMethodNotSet:
        {
            NS_ERROR("How can method not be known when root element is?");
            return NS_ERROR_UNEXPECTED;
        }

        case eXMLOutput:
        case eHTMLOutput:
        {
            *aHandler = new txMozillaXMLOutput(aName, aNsID, aFormat,
                                               mSourceDocument,
                                               mResultDocument,
                                               mObserver);
            break;
        }

        case eTextOutput:
        {
            *aHandler = new txMozillaTextOutput(mSourceDocument,
                                                mResultDocument,
                                                mObserver);
            break;
        }
    }
    NS_ENSURE_TRUE(*aHandler, NS_ERROR_OUT_OF_MEMORY);
    return NS_OK;
}

nsresult
txToFragmentHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                              txAXMLEventHandler** aHandler)
{
    *aHandler = nsnull;
    switch (aFormat->mMethod) {
        case eMethodNotSet:
        {
            txOutputFormat format;
            format.merge(*aFormat);
            nsCOMPtr<nsIDOMDocument> domdoc;
            mFragment->GetOwnerDocument(getter_AddRefs(domdoc));
            NS_ASSERTION(domdoc, "unable to get ownerdocument");
            nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);

            if (!doc || doc->IsCaseSensitive()) {
                format.mMethod = eXMLOutput;
            } else {
                format.mMethod = eHTMLOutput;
            }

            *aHandler = new txMozillaXMLOutput(&format, mFragment);
            break;
        }

        case eXMLOutput:
        case eHTMLOutput:
        {
            *aHandler = new txMozillaXMLOutput(aFormat, mFragment);
            break;
        }

        case eTextOutput:
        {
            *aHandler = new txMozillaTextOutput(mFragment);
            break;
        }
    }
    NS_ENSURE_TRUE(*aHandler, NS_ERROR_OUT_OF_MEMORY);
    return NS_OK;
}

nsresult
txToFragmentHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                              const nsAString& aName,
                                              PRInt32 aNsID,
                                              txAXMLEventHandler** aHandler)
{
    *aHandler = nsnull;
    NS_ASSERTION(aFormat->mMethod != eMethodNotSet,
                 "How can method not be known when root element is?");
    NS_ENSURE_TRUE(aFormat->mMethod != eMethodNotSet, NS_ERROR_UNEXPECTED);
    return createHandlerWith(aFormat, aHandler);
}

/**
 * txMozillaXSLTProcessor
 */

NS_IMPL_ADDREF(txMozillaXSLTProcessor)
NS_IMPL_RELEASE(txMozillaXSLTProcessor)
NS_INTERFACE_MAP_BEGIN(txMozillaXSLTProcessor)
    NS_INTERFACE_MAP_ENTRY(nsIXSLTProcessor)
    NS_INTERFACE_MAP_ENTRY(nsIXSLTProcessorObsolete)
    NS_INTERFACE_MAP_ENTRY(nsIDocumentTransformer)
    NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXSLTProcessor)
    NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XSLTProcessor)
NS_INTERFACE_MAP_END

txMozillaXSLTProcessor::txMozillaXSLTProcessor() : mStylesheetDocument(nsnull),
                                                   mTransformResult(NS_OK),
                                                   mCompileResult(NS_OK),
                                                   mVariables(PR_TRUE)
{
}

txMozillaXSLTProcessor::~txMozillaXSLTProcessor()
{
    if (mStylesheetDocument) {
        mStylesheetDocument->RemoveObserver(this);
    }
}

NS_IMETHODIMP
txMozillaXSLTProcessor::TransformDocument(nsIDOMNode* aSourceDOM,
                                          nsIDOMNode* aStyleDOM,
                                          nsIDOMDocument* aOutputDoc,
                                          nsISupports* aObserver)
{
    NS_ENSURE_ARG(aSourceDOM);
    NS_ENSURE_ARG(aStyleDOM);
    NS_ENSURE_ARG(aOutputDoc);
    NS_ENSURE_FALSE(aObserver, NS_ERROR_NOT_IMPLEMENTED);

    if (!URIUtils::CanCallerAccess(aSourceDOM) ||
        !URIUtils::CanCallerAccess(aStyleDOM) ||
        !URIUtils::CanCallerAccess(aOutputDoc)) {
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    PRUint16 type = 0;
    aStyleDOM->GetNodeType(&type);
    NS_ENSURE_TRUE(type == nsIDOMNode::ELEMENT_NODE ||
                   type == nsIDOMNode::DOCUMENT_NODE,
                   NS_ERROR_INVALID_ARG);

    nsresult rv = TX_CompileStylesheet(aStyleDOM, getter_AddRefs(mStylesheet));
    NS_ENSURE_SUCCESS(rv, rv);

    mSource = aSourceDOM;

    nsAutoPtr<txXPathNode> sourceNode(txXPathNativeNode::createXPathNode(aSourceDOM));
    if (!sourceNode) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIDOMDocument> sourceDOMDocument;
    aSourceDOM->GetOwnerDocument(getter_AddRefs(sourceDOMDocument));
    if (!sourceDOMDocument) {
        sourceDOMDocument = do_QueryInterface(aSourceDOM);
    }

    txExecutionState es(mStylesheet);

    txToDocHandlerFactory handlerFactory(&es, sourceDOMDocument, aOutputDoc,
                                         nsnull);
    es.mOutputHandlerFactory = &handlerFactory;

    es.init(*sourceNode, &mVariables);

    // Process root of XML source document
    rv = txXSLTProcessor::execute(es);
    // XXX setup exception context, bug 204658
    es.end();

    return rv;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::SetTransformObserver(nsITransformObserver* aObserver)
{
    mObserver = aObserver;
    return NS_OK;
}

nsresult
txMozillaXSLTProcessor::SetSourceContentModel(nsIDOMNode* aSourceDOM)
{
    mSource = aSourceDOM;

    if (NS_FAILED(mTransformResult)) {
        notifyError();
        return NS_OK;
    }

    if (mStylesheet) {
        return DoTransform();
    }

    return NS_OK;
}

nsresult
txMozillaXSLTProcessor::DoTransform()
{
    NS_ENSURE_TRUE(mSource, NS_ERROR_UNEXPECTED);
    NS_ENSURE_TRUE(mStylesheet, NS_ERROR_UNEXPECTED);
    NS_ASSERTION(mObserver, "no observer");

    nsAutoPtr<txXPathNode> sourceNode(txXPathNativeNode::createXPathNode(mSource));
    if (!sourceNode) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIDOMDocument> sourceDOMDocument;
    mSource->GetOwnerDocument(getter_AddRefs(sourceDOMDocument));
    if (!sourceDOMDocument) {
        sourceDOMDocument = do_QueryInterface(mSource);
    }

    txExecutionState es(mStylesheet);

    // XXX Need to add error observers

    txToDocHandlerFactory handlerFactory(&es, sourceDOMDocument, nsnull,
                                         mObserver);
    es.mOutputHandlerFactory = &handlerFactory;

    es.init(*sourceNode, &mVariables);

    // Process root of XML source document
    nsresult rv = txXSLTProcessor::execute(es);
    if (NS_FAILED(rv) && mObserver) {
        // XXX set up context information, bug 204655
        reportError(rv, nsnull, nsnull);
    }
    es.end();

    return rv;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::ImportStylesheet(nsIDOMNode *aStyle)
{
    NS_ENSURE_TRUE(aStyle, NS_ERROR_NULL_POINTER);
    
    // We don't support importing multiple stylesheets yet.
    NS_ENSURE_TRUE(!mStylesheetDocument && !mStylesheet,
                   NS_ERROR_NOT_IMPLEMENTED);

    if (!URIUtils::CanCallerAccess(aStyle)) {
        return NS_ERROR_DOM_SECURITY_ERR;
    }
    
    PRUint16 type = 0;
    aStyle->GetNodeType(&type);
    NS_ENSURE_TRUE(type == nsIDOMNode::ELEMENT_NODE ||
                   type == nsIDOMNode::DOCUMENT_NODE,
                   NS_ERROR_INVALID_ARG);

    nsresult rv = TX_CompileStylesheet(aStyle, getter_AddRefs(mStylesheet));
    // XXX set up exception context, bug 204658
    NS_ENSURE_SUCCESS(rv, rv);

    if (type == nsIDOMNode::ELEMENT_NODE) {
        nsCOMPtr<nsIDOMDocument> domDoc;
        aStyle->GetOwnerDocument(getter_AddRefs(domDoc));
        NS_ENSURE_TRUE(domDoc, NS_ERROR_UNEXPECTED);

        nsCOMPtr<nsIDocument> styleDoc = do_QueryInterface(domDoc);
        mStylesheetDocument = styleDoc;
        mEmbeddedStylesheetRoot = do_QueryInterface(aStyle);
    }
    else {
        nsCOMPtr<nsIDocument> styleDoc = do_QueryInterface(aStyle);
        mStylesheetDocument = styleDoc;
    }

    mStylesheetDocument->AddObserver(this);

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::TransformToDocument(nsIDOMNode *aSource,
                                            nsIDOMDocument **aResult)
{
    NS_ENSURE_ARG(aSource);
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_SUCCESS(mCompileResult, mCompileResult);

    if (!URIUtils::CanCallerAccess(aSource)) {
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsresult rv = ensureStylesheet();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txXPathNode> sourceNode(txXPathNativeNode::createXPathNode(aSource));
    if (!sourceNode) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIDOMDocument> sourceDOMDocument;
    aSource->GetOwnerDocument(getter_AddRefs(sourceDOMDocument));
    if (!sourceDOMDocument) {
        sourceDOMDocument = do_QueryInterface(aSource);
    }

    txExecutionState es(mStylesheet);

    // XXX Need to add error observers

    txToDocHandlerFactory handlerFactory(&es, sourceDOMDocument, nsnull,
                                         nsnull);
    es.mOutputHandlerFactory = &handlerFactory;

    es.init(*sourceNode, &mVariables);

    // Process root of XML source document
    rv = txXSLTProcessor::execute(es);
    // XXX setup exception context, bug 204658
    es.end();

    if (NS_SUCCEEDED(rv)) {
        txAOutputXMLEventHandler* handler =
            NS_STATIC_CAST(txAOutputXMLEventHandler*, es.mOutputHandler);
        handler->getOutputDocument(aResult);
    }

    return rv;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::TransformToFragment(nsIDOMNode *aSource,
                                            nsIDOMDocument *aOutput,
                                            nsIDOMDocumentFragment **aResult)
{
    NS_ENSURE_ARG(aSource);
    NS_ENSURE_ARG(aOutput);
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_SUCCESS(mCompileResult, mCompileResult);

    if (!URIUtils::CanCallerAccess(aSource) ||
        !URIUtils::CanCallerAccess(aOutput)) {
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsresult rv = ensureStylesheet();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txXPathNode> sourceNode(txXPathNativeNode::createXPathNode(aSource));
    if (!sourceNode) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    txExecutionState es(mStylesheet);

    // XXX Need to add error observers

    rv = aOutput->CreateDocumentFragment(aResult);
    NS_ENSURE_SUCCESS(rv, rv);
    txToFragmentHandlerFactory handlerFactory(*aResult);
    es.mOutputHandlerFactory = &handlerFactory;

    es.init(*sourceNode, &mVariables);

    // Process root of XML source document
    rv = txXSLTProcessor::execute(es);
    // XXX setup exception context, bug 204658
    es.end();

    return rv;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::SetParameter(const nsAString & aNamespaceURI,
                                     const nsAString & aLocalName,
                                     nsIVariant *aValue)
{
    NS_ENSURE_ARG(aValue);
    PRUint16 dataType;
    aValue->GetDataType(&dataType);
    switch (dataType) {
        // Number
        case nsIDataType::VTYPE_INT8:
        case nsIDataType::VTYPE_INT16:
        case nsIDataType::VTYPE_INT32:
        case nsIDataType::VTYPE_INT64:
        case nsIDataType::VTYPE_UINT8:
        case nsIDataType::VTYPE_UINT16:
        case nsIDataType::VTYPE_UINT32:
        case nsIDataType::VTYPE_UINT64:
        case nsIDataType::VTYPE_FLOAT:
        case nsIDataType::VTYPE_DOUBLE:

        // Boolean
        case nsIDataType::VTYPE_BOOL:

        // String
        case nsIDataType::VTYPE_CHAR:
        case nsIDataType::VTYPE_WCHAR:
        case nsIDataType::VTYPE_DOMSTRING:
        case nsIDataType::VTYPE_CHAR_STR:
        case nsIDataType::VTYPE_WCHAR_STR:
        case nsIDataType::VTYPE_STRING_SIZE_IS:
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        case nsIDataType::VTYPE_UTF8STRING:
        case nsIDataType::VTYPE_CSTRING:
        case nsIDataType::VTYPE_ASTRING:

        // Nodeset
        case nsIDataType::VTYPE_INTERFACE:
        case nsIDataType::VTYPE_INTERFACE_IS:
        case nsIDataType::VTYPE_ARRAY:
        {
            // This might still be an error, but we'll only
            // find out later since we lazily evaluate.
            break;
        }

        default:
        {
            return NS_ERROR_FAILURE;
        }        
    }

    PRInt32 nsId = kNameSpaceID_Unknown;
    nsresult rv = gTxNameSpaceManager->RegisterNameSpace(aNamespaceURI, nsId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIAtom> localName = do_GetAtom(aLocalName);
    txExpandedName varName(nsId, localName);

    txVariable* var = (txVariable*)mVariables.get(varName);
    if (var) {
        var->setValue(aValue);
        return NS_OK;
    }

    var = new txVariable(aValue);
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);

    return mVariables.add(varName, var);
}

NS_IMETHODIMP
txMozillaXSLTProcessor::GetParameter(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIVariant **aResult)
{
    PRInt32 nsId = kNameSpaceID_Unknown;
    nsresult rv = gTxNameSpaceManager->RegisterNameSpace(aNamespaceURI, nsId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIAtom> localName = do_GetAtom(aLocalName);
    txExpandedName varName(nsId, localName);

    txVariable* var = (txVariable*)mVariables.get(varName);
    if (var) {
        return var->getValue(aResult);
    }
    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::RemoveParameter(const nsAString& aNamespaceURI,
                                        const nsAString& aLocalName)
{
    PRInt32 nsId = kNameSpaceID_Unknown;
    nsresult rv = gTxNameSpaceManager->RegisterNameSpace(aNamespaceURI, nsId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIAtom> localName = do_GetAtom(aLocalName);
    txExpandedName varName(nsId, localName);

    mVariables.remove(varName);
    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::ClearParameters()
{
    mVariables.clear();

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::Reset()
{
    if (mStylesheetDocument) {
        mStylesheetDocument->RemoveObserver(this);
    }
    mStylesheet = nsnull;
    mStylesheetDocument = nsnull;
    mEmbeddedStylesheetRoot = nsnull;
    mCompileResult = NS_OK;
    mVariables.clear();

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXSLTProcessor::LoadStyleSheet(nsIURI* aUri, nsILoadGroup* aLoadGroup,
                                       nsIPrincipal* aCallerPrincipal)
{
    nsresult rv = TX_LoadSheet(aUri, this, aLoadGroup, aCallerPrincipal);
    if (NS_FAILED(rv) && mObserver) {
        // This is most likely a network or security error, just
        // use the uri as context.
        nsCAutoString spec;
        if (aUri) {
            aUri->GetSpec(spec);
            CopyUTF8toUTF16(spec, mSourceText);
        }
        reportError(rv, nsnull, nsnull);
    }
    return rv;
}

nsresult
txMozillaXSLTProcessor::setStylesheet(txStylesheet* aStylesheet)
{
    mStylesheet = aStylesheet;
    if (mSource) {
        return DoTransform();
    }
    return NS_OK;
}

void
txMozillaXSLTProcessor::reportError(nsresult aResult,
                                    const PRUnichar *aErrorText,
                                    const PRUnichar *aSourceText)
{
    if (!mObserver) {
        return;
    }

    mTransformResult = aResult;

    if (aErrorText) {
        mErrorText.Assign(aErrorText);
    }
    else {
        nsCOMPtr<nsIStringBundleService> sbs =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID);
        if (sbs) {
            nsXPIDLString errorText;
            sbs->FormatStatusMessage(aResult, EmptyString().get(),
                                     getter_Copies(errorText));

            nsXPIDLString errorMessage;
            nsCOMPtr<nsIStringBundle> bundle;
            sbs->CreateBundle(XSLT_MSGS_URL, getter_AddRefs(bundle));

            if (bundle) {
                const PRUnichar* error[] = { errorText.get() };
                if (mStylesheet) {
                    bundle->FormatStringFromName(NS_LITERAL_STRING("TransformError").get(),
                                                 error, 1,
                                                 getter_Copies(errorMessage));
                }
                else {
                    bundle->FormatStringFromName(NS_LITERAL_STRING("LoadingError").get(),
                                                 error, 1,
                                                 getter_Copies(errorMessage));
                }
            }
            mErrorText.Assign(errorMessage);
        }
    }

    if (aSourceText) {
        mSourceText.Assign(aSourceText);
    }

    if (mSource) {
        notifyError();
    }
}

void
txMozillaXSLTProcessor::notifyError()
{
    nsresult rv;
    nsCOMPtr<nsIDOMDocument> errorDocument = do_CreateInstance(kXMLDocumentCID,
                                                               &rv);
    if (NS_FAILED(rv)) {
        return;
    }

    // Set up the document
    nsCOMPtr<nsIDocument> document = do_QueryInterface(errorDocument);
    if (!document) {
        return;
    }
    URIUtils::ResetWithSource(document, mSource);

    NS_NAMED_LITERAL_STRING(ns, "http://www.mozilla.org/newlayout/xml/parsererror.xml");

    nsCOMPtr<nsIDOMElement> element;
    rv = errorDocument->CreateElementNS(ns, NS_LITERAL_STRING("parsererror"),
                                        getter_AddRefs(element));
    if (NS_FAILED(rv)) {
        return;
    }

    nsCOMPtr<nsIContent> rootContent = do_QueryInterface(element);
    if (!rootContent) {
        return;
    }

    rootContent->SetDocument(document, PR_FALSE, PR_TRUE);
    document->SetRootContent(rootContent);

    nsCOMPtr<nsIDOMText> text;
    rv = errorDocument->CreateTextNode(mErrorText, getter_AddRefs(text));
    if (NS_FAILED(rv)) {
        return;
    }

    nsCOMPtr<nsIDOMNode> resultNode;
    rv = element->AppendChild(text, getter_AddRefs(resultNode));
    if (NS_FAILED(rv)) {
        return;
    }

    if (!mSourceText.IsEmpty()) {
        nsCOMPtr<nsIDOMElement> sourceElement;
        rv = errorDocument->CreateElementNS(ns,
                                            NS_LITERAL_STRING("sourcetext"),
                                            getter_AddRefs(sourceElement));
        if (NS_FAILED(rv)) {
            return;
        }
    
        rv = element->AppendChild(sourceElement, getter_AddRefs(resultNode));
        if (NS_FAILED(rv)) {
            return;
        }

        rv = errorDocument->CreateTextNode(mSourceText, getter_AddRefs(text));
        if (NS_FAILED(rv)) {
            return;
        }
    
        rv = sourceElement->AppendChild(text, getter_AddRefs(resultNode));
        if (NS_FAILED(rv)) {
            return;
        }
    }

    mObserver->OnTransformDone(mTransformResult, errorDocument);
}

nsresult
txMozillaXSLTProcessor::ensureStylesheet()
{
    if (mStylesheet) {
        return NS_OK;
    }

    NS_ENSURE_TRUE(mStylesheetDocument, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDOMNode> style = do_QueryInterface(mEmbeddedStylesheetRoot);
    if (!style) {
        style = do_QueryInterface(mStylesheetDocument);
    }
    return TX_CompileStylesheet(style, getter_AddRefs(mStylesheet));
}

NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(txMozillaXSLTProcessor)
NS_IMPL_NSIDOCUMENTOBSERVER_REFLOW_STUB(txMozillaXSLTProcessor)
NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(txMozillaXSLTProcessor)
NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(txMozillaXSLTProcessor)

void
txMozillaXSLTProcessor::BeginUpdate(nsIDocument* aDocument,
                                    nsUpdateType aUpdateType)
{
}

void
txMozillaXSLTProcessor::EndUpdate(nsIDocument* aDocument,
                                  nsUpdateType aUpdateType)
{
}

void
txMozillaXSLTProcessor::DocumentWillBeDestroyed(nsIDocument* aDocument)
{
    if (NS_FAILED(mCompileResult)) {
        return;
    }

    mCompileResult = ensureStylesheet();
    mStylesheetDocument = nsnull;
    mEmbeddedStylesheetRoot = nsnull;

    // This might not be neccesary, but just in case some element ends up
    // causing a notification as the document goes away we don't want to
    // invalidate the stylesheet.
    aDocument->RemoveObserver(this);
}

void
txMozillaXSLTProcessor::CharacterDataChanged(nsIDocument* aDocument,
                                             nsIContent *aContent,
                                             PRBool aAppend)
{
    mStylesheet = nsnull;
}

void
txMozillaXSLTProcessor::AttributeChanged(nsIDocument* aDocument,
                                         nsIContent* aContent,
                                         PRInt32 aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         PRInt32 aModType)
{
    mStylesheet = nsnull;
}

void
txMozillaXSLTProcessor::ContentAppended(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        PRInt32 aNewIndexInContainer)
{
    mStylesheet = nsnull;
}

void
txMozillaXSLTProcessor::ContentInserted(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aChild,
                                        PRInt32 aIndexInContainer)
{
    mStylesheet = nsnull;
}

void
txMozillaXSLTProcessor::ContentRemoved(nsIDocument* aDocument,
                                       nsIContent* aContainer,
                                       nsIContent* aChild,
                                       PRInt32 aIndexInContainer)
{
    mStylesheet = nsnull;
}

/* static*/
nsresult
txVariable::Convert(nsIVariant *aValue, txAExprResult** aResult)
{
    *aResult = nsnull;

    PRUint16 dataType;
    aValue->GetDataType(&dataType);
    switch (dataType) {
        // Number
        case nsIDataType::VTYPE_INT8:
        case nsIDataType::VTYPE_INT16:
        case nsIDataType::VTYPE_INT32:
        case nsIDataType::VTYPE_INT64:
        case nsIDataType::VTYPE_UINT8:
        case nsIDataType::VTYPE_UINT16:
        case nsIDataType::VTYPE_UINT32:
        case nsIDataType::VTYPE_UINT64:
        case nsIDataType::VTYPE_FLOAT:
        case nsIDataType::VTYPE_DOUBLE:
        {
            double value;
            nsresult rv = aValue->GetAsDouble(&value);
            NS_ENSURE_SUCCESS(rv, rv);

            *aResult = new NumberResult(value, nsnull);
            NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

            NS_ADDREF(*aResult);

            return NS_OK;
        }

        // Boolean
        case nsIDataType::VTYPE_BOOL:
        {
            PRBool value;
            nsresult rv = aValue->GetAsBool(&value);
            NS_ENSURE_SUCCESS(rv, rv);

            *aResult = new BooleanResult(value);
            NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

            NS_ADDREF(*aResult);

            return NS_OK;
        }

        // String
        case nsIDataType::VTYPE_CHAR:
        case nsIDataType::VTYPE_WCHAR:
        case nsIDataType::VTYPE_DOMSTRING:
        case nsIDataType::VTYPE_CHAR_STR:
        case nsIDataType::VTYPE_WCHAR_STR:
        case nsIDataType::VTYPE_STRING_SIZE_IS:
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        case nsIDataType::VTYPE_UTF8STRING:
        case nsIDataType::VTYPE_CSTRING:
        case nsIDataType::VTYPE_ASTRING:
        {
            nsAutoString value;
            nsresult rv = aValue->GetAsAString(value);
            NS_ENSURE_SUCCESS(rv, rv);

            *aResult = new StringResult(value, nsnull);
            NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

            NS_ADDREF(*aResult);

            return NS_OK;
        }

        // Nodeset
        case nsIDataType::VTYPE_INTERFACE:
        case nsIDataType::VTYPE_INTERFACE_IS:
        {
            nsID *iid;
            nsCOMPtr<nsISupports> supports;
            nsresult rv = aValue->GetAsInterface(&iid, getter_AddRefs(supports));
            NS_ENSURE_SUCCESS(rv, rv);
            if (iid) {
                // XXX Figure out what the user added and if we can do
                //     anything with it.
                //     nsIDOMNode, nsIDOMNodeList, nsIDOMXPathResult
                nsMemory::Free(iid);
            }
            break;
        }

        case nsIDataType::VTYPE_ARRAY:
        {
            // XXX Figure out what the user added and if we can do
            //     anything with it. Array of Nodes. 
            break;
        }
    }
    return NS_ERROR_ILLEGAL_VALUE;
}
