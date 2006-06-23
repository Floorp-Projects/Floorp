/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *  Olli Pettay <Olli.Pettay@helsinki.fi>
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

#include "nsXFormsInstanceElement.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOM3Node.h"
#include "nsMemory.h"
#include "nsXFormsAtoms.h"
#include "nsString.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIXTFGenericElementWrapper.h"
#include "nsXFormsUtils.h"
#include "nsNetUtil.h"

static const char* kLoadAsData = "loadAsData";

NS_IMPL_ISUPPORTS_INHERITED6(nsXFormsInstanceElement,
                             nsXFormsStubElement,
                             nsIXFormsNSInstanceElement,
                             nsIInstanceElementPrivate,
                             nsIStreamListener,
                             nsIRequestObserver,
                             nsIInterfaceRequestor,
                             nsIChannelEventSink)

nsXFormsInstanceElement::nsXFormsInstanceElement()
  : mElement(nsnull)
  , mInitialized(PR_FALSE)
  , mLazy(PR_FALSE)
{
}

NS_IMETHODIMP
nsXFormsInstanceElement::OnDestroyed()
{
  if (mChannel) {
    // better be a good citizen and tell the browser that we don't need this
    // resource anymore.
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }
  mListener = nsnull;
  SetInstanceDocument(nsnull);
  mElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::AttributeSet(nsIAtom *aName,
                                      const nsAString &aNewValue)
{
  if (!mInitialized || mLazy)
    return NS_OK;

  if (aName == nsXFormsAtoms::src) {
    LoadExternalInstance(aNewValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::AttributeRemoved(nsIAtom *aName)
{
  if (!mInitialized || mLazy)
    return NS_OK;

  if (aName == nsXFormsAtoms::src) {
    PRBool restart = PR_FALSE;
    if (mChannel) {
      // looks like we are trying to remove the src attribute while we are
      // already trying to load the external instance document.  We'll stop
      // the current load effort.
      restart = PR_TRUE;
      mChannel->Cancel(NS_BINDING_ABORTED);
      mChannel = nsnull;
      mListener = nsnull;
    }
    // We no longer have an external instance to use.  Reset our instance
    // document to whatever inline content we have.
    nsresult rv = CloneInlineInstance();

    // if we had already started to load an external instance document, then
    // as part of that we would have told the model to wait for that external
    // document to load before it finishes the model construction.  Since we
    // aren't loading from an external document any longer, tell the model that
    // there is need to wait for us anymore.
    if (restart) {
      nsCOMPtr<nsIModelElementPrivate> model = GetModel();
      if (model) {
        model->InstanceLoadFinished(PR_TRUE);
      }
    }
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::OnCreated(nsIXTFGenericElementWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                nsIXTFElement::NOTIFY_ATTRIBUTE_REMOVED |
                                nsIXTFElement::NOTIFY_WILL_CHANGE_PARENT |
                                nsIXTFElement::NOTIFY_PARENT_CHANGED);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a weak pointer to mElement.  mElement will have an owning
  // reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  return NS_OK;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
nsXFormsInstanceElement::GetInterface(const nsIID & aIID, void **aResult)
{
  *aResult = nsnull;
  return QueryInterface(aIID, aResult);
}

// nsIChannelEventSink

NS_IMETHODIMP
nsXFormsInstanceElement::OnChannelRedirect(nsIChannel *OldChannel,
                                           nsIChannel *aNewChannel,
                                           PRUint32    aFlags)
{
  NS_PRECONDITION(aNewChannel, "Redirect without a channel?");
  NS_PRECONDITION(!mLazy, "Loading an instance document for a lazy instance?");

  nsCOMPtr<nsIURI> newURI;
  nsresult rv = aNewChannel->GetURI(getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsXFormsUtils::CheckConnectionAllowed(mElement, newURI)) {
    const PRUnichar *strings[] = { NS_LITERAL_STRING("instance").get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLinkLoadOrigin"),
                               strings, 1, mElement, mElement);
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

// nsIStreamListener

// It is possible that mListener could be null here.  If the document hit a
// parsing error after we've already started to load the external source, then
// Mozilla will destroy all of the elements and the document in order to load
// the parser error page.  This will cause mListener to become null but since
// the channel will hold a nsCOMPtr to the nsXFormsInstanceElement preventing
// it from being freed up, the channel will still be able to call the
// nsIStreamListener functions that we implement here.  And calling
// mChannel->Cancel() is no guarantee that these other notifications won't come
// through if the timing is wrong.  So we need to check for mElement below
// before we handle any of the stream notifications.

NS_IMETHODIMP
nsXFormsInstanceElement::OnStartRequest(nsIRequest *request, nsISupports *ctx)
{
  NS_PRECONDITION(!mLazy, "Loading an instance document for a lazy instance?");
  if (!mElement || !mListener) {
    return NS_OK;
  }
  return mListener->OnStartRequest(request, ctx);
}

NS_IMETHODIMP
nsXFormsInstanceElement::OnDataAvailable(nsIRequest *aRequest,
                                         nsISupports *ctxt,
                                         nsIInputStream *inStr,
                                         PRUint32 sourceOffset,
                                         PRUint32 count)
{
  NS_PRECONDITION(!mLazy, "Loading an instance document for a lazy instance?");
  if (!mElement || !mListener) {
    return NS_OK;
  }
  return mListener->OnDataAvailable(aRequest, ctxt, inStr, sourceOffset, count);
}

NS_IMETHODIMP
nsXFormsInstanceElement::OnStopRequest(nsIRequest *request, nsISupports *ctx,
                                       nsresult status)
{
  NS_PRECONDITION(!mLazy, "Loading an instance document for a lazy instance?");
  if (status == NS_BINDING_ABORTED) {
    // looks like our element has already been destroyed.  No use continuing on.
    return NS_OK;
  }

  mChannel = nsnull;
  NS_ASSERTION(mListener, "No stream listener for document!");
  mListener->OnStopRequest(request, ctx, status);

  PRBool succeeded = NS_SUCCEEDED(status);
  if (!succeeded) {
    SetInstanceDocument(nsnull);
  }

  if (mDocument) {
    nsCOMPtr<nsIDOMElement> docElem;
    mDocument->GetDocumentElement(getter_AddRefs(docElem));
    if (docElem) {
      nsAutoString tagName, namespaceURI;
      docElem->GetTagName(tagName);
      docElem->GetNamespaceURI(namespaceURI);
  
      if (tagName.EqualsLiteral("parsererror") &&
          namespaceURI.EqualsLiteral("http://www.mozilla.org/newlayout/xml/parsererror.xml")) {
        NS_WARNING("resulting instance document could not be parsed");
        succeeded = PR_FALSE;
        SetInstanceDocument(nsnull);
      }
    }
  }

  // Replace the principal for the loaded document
  nsCOMPtr<nsIDocument> iDoc(do_QueryInterface(mDocument));
  nsresult rv = ReplacePrincipal(iDoc);
  if (NS_FAILED(rv)) {
    SetInstanceDocument(nsnull);
    return rv;
  }

  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  if (model) {
    model->InstanceLoadFinished(succeeded);
  }

  mListener = nsnull;
  return NS_OK;
}

nsresult
nsXFormsInstanceElement::ReplacePrincipal(nsIDocument *aDocument)
{
  if (!aDocument || !mElement)
    return NS_ERROR_FAILURE;

  // Set Principal
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> fromDoc(do_QueryInterface(domDoc));
  NS_ENSURE_STATE(fromDoc);
  aDocument->SetPrincipal(fromDoc->NodePrincipal());

  return NS_OK;
}

// nsIXFormsNSInstanceElement

NS_IMETHODIMP
nsXFormsInstanceElement::GetInstanceDocument(nsIDOMDocument **aDocument)
{
  NS_IF_ADDREF(*aDocument = mDocument);
  return NS_OK;
}

// nsIInstanceElementPrivate

NS_IMETHODIMP
nsXFormsInstanceElement::SetInstanceDocument(nsIDOMDocument *aDocument)
{
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  if (doc) {
    doc->UnsetProperty(nsXFormsAtoms::instanceDocumentOwner);
  }

  mDocument = aDocument;

  doc = do_QueryInterface(mDocument);
  if (doc) {
    // Set property to prevent an instance document loading an external instance
    // document
    nsresult rv = doc->SetProperty(nsXFormsAtoms::isInstanceDocument, doc);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupports> owner(do_QueryInterface(mElement));
    NS_ENSURE_STATE(owner);
    rv = doc->SetProperty(nsXFormsAtoms::instanceDocumentOwner, owner);
    NS_ENSURE_SUCCESS(rv, rv);

    // Replace the principal of the instance document so it is the same as for
    // the owning form. Why is this not a security breach? Because we handle
    // our own whitelist of domains that we trust (see
    // nsXFormsUtils::CheckSameOrigin()), and if we have gotten this far
    // (ie. loaded the document) the user has trusted obviously trusted the
    // source. See also https://bugzilla.mozilla.org/show_bug.cgi?id=338451
    rv = ReplacePrincipal(doc);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXFormsInstanceElement::BackupOriginalDocument()
{
  nsresult rv = NS_OK;

  // This is called when xforms-ready is received by the model.  By now we know 
  // that the instance document, whether external or inline, is loaded into 
  // mDocument.  Get the root node, clone it, and insert it into our copy of 
  // the document.  This is the magic behind getting xforms-reset to work.
  nsCOMPtr<nsIDocument> origDoc(do_QueryInterface(mOriginalDocument));
  if(mDocument && origDoc) {
    // xf:instance elements in original document must not try to load anything.
    rv = origDoc->SetProperty(nsXFormsAtoms::isInstanceDocument, origDoc);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> newNode;
    nsCOMPtr<nsIDOMElement> instanceRoot;
    rv = mDocument->GetDocumentElement(getter_AddRefs(instanceRoot));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(instanceRoot, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNode> nodeReturn;
    rv = instanceRoot->CloneNode(PR_TRUE, getter_AddRefs(newNode)); 
    if(NS_SUCCEEDED(rv)) {
      rv = mOriginalDocument->AppendChild(newNode, getter_AddRefs(nodeReturn));
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), 
                       "failed to set up original instance document");
    }
  }
  return rv;
}

NS_IMETHODIMP
nsXFormsInstanceElement::RestoreOriginalDocument()
{
  // This is called when xforms-reset is received by the model.  We assume
  // that the backup of the instance document has been populated and is
  // loaded into mOriginalDocument.
 
 if (!mOriginalDocument) {
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIDOMNode> newDocNode;
  nsresult rv = mOriginalDocument->CloneNode(PR_TRUE,
                                             getter_AddRefs(newDocNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> newDoc(do_QueryInterface(newDocNode));
  NS_ENSURE_STATE(newDoc);
  
  rv = SetInstanceDocument(newDoc);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::GetElement(nsIDOMElement **aElement)
{
  NS_IF_ADDREF(*aElement = mElement);
  return NS_OK;  
}

NS_IMETHODIMP
nsXFormsInstanceElement::WillChangeParent(nsIDOMElement *aNewParent)
{
  if (!aNewParent) {
    nsCOMPtr<nsIModelElementPrivate> model = GetModel();
    if (model) {
      model->RemoveInstanceElement(this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::ParentChanged(nsIDOMElement *aNewParent)
{
  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  if (!model) return NS_OK;

  if (mInitialized || !mElement) {
    return NS_OK;
  }

  mInitialized = PR_TRUE;
  model->AddInstanceElement(this);

  // If model isn't loaded entirely (It means xforms-ready event hasn't been
  // fired) then the instance will be initialized by model and will be backed up
  // when xforms-ready event is fired.
  PRBool isready;
  model->GetIsReady(&isready);
  if (!isready)
    return NS_OK;

  // If the model is loaded and ready then the instance is inserted dynamically.
  // The instance should create instance document and back it up.

  // Probably dynamic instances should be handled too when model isn't loaded
  // entirely (for more information see a comment 29 of bug 320081
  // https://bugzilla.mozilla.org/show_bug.cgi?id=320081#c29).

  nsAutoString src;
  mElement->GetAttribute(NS_LITERAL_STRING("src"), src);

  if (!src.IsEmpty()) {
    // XXX: external dynamic instances isn't handled (see a bug 325684
    // https://bugzilla.mozilla.org/show_bug.cgi?id=325684)
    return NS_OK;
  }

  // If we don't have a linked external instance, use our inline data.
  nsresult rv = CloneInlineInstance();
  if (NS_FAILED(rv))
    return rv;
  return BackupOriginalDocument();
}

NS_IMETHODIMP
nsXFormsInstanceElement::Initialize()
{
  mElement->HasAttributeNS(NS_LITERAL_STRING(NS_NAMESPACE_MOZ_XFORMS_LAZY),
                           NS_LITERAL_STRING("lazy"), &mLazy);

  if (mLazy) { // Lazy instance
    return CreateInstanceDocument(NS_LITERAL_STRING("instanceData"));
  }

  // Normal instance
  nsAutoString src;
  mElement->GetAttribute(NS_LITERAL_STRING("src"), src);

  if (src.IsEmpty()) {
    return CloneInlineInstance();
  }

  LoadExternalInstance(src);
  return NS_OK;
}

// private methods

nsresult
nsXFormsInstanceElement::CloneInlineInstance()
{
  // Clear out our existing instance data
  nsresult rv = CreateInstanceDocument(EmptyString());
  if (NS_FAILED(rv))
    return rv; // don't warn, we might just not be in the document yet

  // look for our first child element (skip over text nodes, etc.)
  nsCOMPtr<nsIDOMNode> child, temp;
  mElement->GetFirstChild(getter_AddRefs(child));
  while (child) {
    PRUint16 nodeType;
    child->GetNodeType(&nodeType);

    if (nodeType == nsIDOMNode::ELEMENT_NODE)
      break;

    temp.swap(child);
    temp->GetNextSibling(getter_AddRefs(child));
  }

  // There needs to be a child element, or it is not a valid XML document
  if (!child) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("inlineInstanceNoChildError"),
                               mElement);
    nsCOMPtr<nsIModelElementPrivate> model(GetModel());
    nsCOMPtr<nsIDOMNode> modelNode(do_QueryInterface(model));
    nsXFormsUtils::DispatchEvent(modelNode, eEvent_LinkException);
    nsXFormsUtils::HandleFatalError(mElement,
                                    NS_LITERAL_STRING("XFormsLinkException"));
    return NS_ERROR_FAILURE;
  }

  // Check for siblings to first child element node. This is an error, since
  // the inline content is then not a well-formed XML document.
  nsCOMPtr<nsIDOMNode> sibling;
  child->GetNextSibling(getter_AddRefs(sibling));
  while (sibling) {
    PRUint16 nodeType;
    sibling->GetNodeType(&nodeType);

    if (nodeType == nsIDOMNode::ELEMENT_NODE) {
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("inlineInstanceMultipleElementsError"),
                                 mElement);
      nsCOMPtr<nsIModelElementPrivate> model(GetModel());
      nsCOMPtr<nsIDOMNode> modelNode(do_QueryInterface(model));
      nsXFormsUtils::DispatchEvent(modelNode, eEvent_LinkException);
      nsXFormsUtils::HandleFatalError(mElement,
                                      NS_LITERAL_STRING("XFormsLinkException"));
      return NS_ERROR_FAILURE;
    }

    temp.swap(sibling);
    temp->GetNextSibling(getter_AddRefs(sibling));
  }

  // Clone and insert content into new document
  nsCOMPtr<nsIDOMNode> newNode;
  rv = mDocument->ImportNode(child, PR_TRUE, getter_AddRefs(newNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> nodeReturn;
  rv = mDocument->AppendChild(newNode, getter_AddRefs(nodeReturn));
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "failed to append root instance node");

  return rv;
}

void
nsXFormsInstanceElement::LoadExternalInstance(const nsAString &aSrc)
{
  nsresult rv = NS_ERROR_FAILURE;

  PRBool restart = PR_FALSE;
  if (mChannel) {
    // probably hit this condition because someone changed the value of our
    // src attribute while we are already trying to load the previously
    // specified document.  We'll stop the current load effort and kick off the
    // new attempt.
    restart = PR_TRUE;
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
    mListener = nsnull;
  }

  // Check whether we are an instance document ourselves
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (doc) {
    if (doc->GetProperty(nsXFormsAtoms::isInstanceDocument)) {
      /// Property exists, which means we are an instance document trying
      /// to load an external instance document. We do not allow that.
      const nsPromiseFlatString& flat = PromiseFlatString(aSrc);
      const PRUnichar *strings[] = { flat.get() };
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("instanceInstanceLoad"),
                                 strings, 1, mElement, mElement);
    } else {
      // Clear out our existing instance data
      if (NS_SUCCEEDED(CreateInstanceDocument(EmptyString()))) {
        nsCOMPtr<nsIDocument> newDoc = do_QueryInterface(mDocument);

        nsCOMPtr<nsIURI> uri;
        NS_NewURI(getter_AddRefs(uri), aSrc,
                  doc->GetDocumentCharacterSet().get(), doc->GetDocumentURI());
        if (uri) {
          if (nsXFormsUtils::CheckConnectionAllowed(mElement, uri)) {
            nsCOMPtr<nsILoadGroup> loadGroup;
            loadGroup = doc->GetDocumentLoadGroup();
            NS_WARN_IF_FALSE(loadGroup, "No load group!");

            // Using the same load group as the main document and creating
            // the channel with LOAD_NORMAL flag delays the dispatching of
            // the 'load' event until all instance data documents have been
            // loaded.
            NS_NewChannel(getter_AddRefs(mChannel), uri, nsnull, loadGroup,
                          nsnull, nsIRequest::LOAD_NORMAL);

            if (mChannel) {
              rv = newDoc->StartDocumentLoad(kLoadAsData, mChannel, loadGroup,
                                             nsnull, getter_AddRefs(mListener),
                                             PR_TRUE);
              if (NS_SUCCEEDED(rv)) {
                mChannel->SetNotificationCallbacks(this);
                rv = mChannel->AsyncOpen(this, nsnull);
              }
            }
          } else {
            const PRUnichar *strings[] = {NS_LITERAL_STRING("instance").get()};
            nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLinkLoadOrigin"),
                                       strings, 1, mElement, mElement);
          }
        }
      }
    }
  }

  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  if (model) {
    // if this isn't the first time that this instance element has tried
    // to load an external document, then we don't need to tell the model
    // to wait again.  It would screw up the counter it uses.  But if this
    // isn't a restart, then by golly, the model better wait for us!
    if (!restart) {
      model->InstanceLoadStarted();
    }
    if (NS_FAILED(rv)) {
      model->InstanceLoadFinished(PR_FALSE);
    }
  }
}

nsresult
nsXFormsInstanceElement::CreateInstanceDocument(const nsAString &aQualifiedName)
{
  nsCOMPtr<nsIDOMDocument> doc;
  nsresult rv = mElement->GetOwnerDocument(getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!doc) // could be we just aren't inserted yet, so don't warn
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDOMImplementation> domImpl;
  rv = doc->GetImplementation(getter_AddRefs(domImpl));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> newDoc;
  rv = domImpl->CreateDocument(EmptyString(), aQualifiedName, nsnull,
                               getter_AddRefs(newDoc));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetInstanceDocument(newDoc);
  NS_ENSURE_SUCCESS(rv, rv);

  // I don't know if not being able to create a backup document is worth
  // failing this function.  Since it probably won't be used often, we'll
  // let it slide.  But it probably does mean that things are going south
  // with the browser.
  domImpl->CreateDocument(EmptyString(), aQualifiedName, nsnull,
                          getter_AddRefs(mOriginalDocument));
  return rv;
}

already_AddRefed<nsIModelElementPrivate>
nsXFormsInstanceElement::GetModel()
{
  if (!mElement)
  {
    NS_WARNING("The XTF wrapper element has been destroyed");
    return nsnull;
  }

  nsCOMPtr<nsIDOMNode> parentNode;
  mElement->GetParentNode(getter_AddRefs(parentNode));

  nsIModelElementPrivate *model = nsnull;
  if (parentNode)
    CallQueryInterface(parentNode, &model);
  return model;
}

nsresult
NS_NewXFormsInstanceElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInstanceElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
