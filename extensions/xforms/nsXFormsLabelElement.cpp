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

/**
 * Implementation of the XForms \<label\> element.
 */

#include "nsXFormsUtils.h"
#include "nsXFormsControlStub.h"
#include "nsXFormsDelegateStub.h"
#include "nsXFormsAtoms.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsString.h"
#include "nsIXFormsUIWidget.h"
#include "nsIDocument.h"
#include "nsNetUtil.h"
#include "nsIXFormsItemElement.h"

class nsXFormsLabelElement : public nsXFormsDelegateStub,
                             public nsIStreamListener,
                             public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsIXFormsDelegate
  NS_IMETHOD GetValue(nsAString& aValue);
  NS_IMETHOD WidgetAttached();

  // nsIXFormsControl
  NS_IMETHOD IsEventTarget(PRBool *aOK);
  NS_IMETHOD Refresh();

  NS_IMETHOD OnCreated(nsIXTFElementWrapper *aWrapper);
  NS_IMETHOD OnDestroyed();

  // nsIXTFElement overrides
  NS_IMETHOD ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex);
  NS_IMETHOD ChildAppended(nsIDOMNode *aChild);
  NS_IMETHOD ChildRemoved(PRUint32 aIndex);
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aSrc);
  NS_IMETHOD AttributeRemoved(nsIAtom *aName);
  NS_IMETHOD PerformAccesskey();

  nsXFormsLabelElement() : mWidgetLoaded(PR_FALSE) {};

#ifdef DEBUG_smaug
  virtual const char* Name() { return "label"; }
#endif
private:
  NS_HIDDEN_(void) LoadExternalLabel(const nsAString& aValue);

  nsCString            mSrcAttrText;
  nsCOMPtr<nsIChannel> mChannel;
  PRBool               mWidgetLoaded;
};

NS_IMPL_ISUPPORTS_INHERITED3(nsXFormsLabelElement,
                             nsXFormsDelegateStub,
                             nsIRequestObserver,
                             nsIStreamListener,
                             nsIInterfaceRequestor)

NS_IMETHODIMP
nsXFormsLabelElement::OnCreated(nsIXTFElementWrapper *aWrapper)
{
  nsresult rv = nsXFormsDelegateStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  aWrapper->SetNotificationMask(kStandardNotificationMask |
                                nsIXTFElement::NOTIFY_CHILD_INSERTED |
                                nsIXTFElement::NOTIFY_CHILD_APPENDED |
                                nsIXTFElement::NOTIFY_CHILD_REMOVED |
                                nsIXTFElement::NOTIFY_PERFORM_ACCESSKEY);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::OnDestroyed()
{
  if (mChannel) {
    // better be a good citizen and tell the browser that we don't need this
    // resource anymore.
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }
  return nsXFormsDelegateStub::OnDestroyed();
}

NS_IMETHODIMP
nsXFormsLabelElement::ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex)
{
  Refresh();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::ChildAppended(nsIDOMNode *aChild)
{
  Refresh();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::ChildRemoved(PRUint32 aIndex)
{
  Refresh();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::src) {
    // If we are currently trying to load an external label, cancel the request.
    if (mChannel) {
      mChannel->Cancel(NS_BINDING_ABORTED);
    }

    LoadExternalLabel(aValue);

    // No need to call Refresh() here, since it is called once the link
    // target has been read in, during OnStopRequest()

    return NS_OK;
  }

  return nsXFormsDelegateStub::AttributeSet(aName, aValue);
}

NS_IMETHODIMP
nsXFormsLabelElement::AttributeRemoved(nsIAtom *aName)
{
  if (aName == nsXFormsAtoms::src) {
    // If we are currently trying to load an external label, cancel the request.
    if (mChannel) {
      mChannel->Cancel(NS_BINDING_ABORTED);
    }

    mSrcAttrText.Truncate();
    Refresh();
    return NS_OK;
  }

  return nsXFormsDelegateStub::AttributeRemoved(aName);
}

NS_IMETHODIMP
nsXFormsLabelElement::PerformAccesskey()
{
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mElement));
  nsCOMPtr<nsIDOMNode> parent;
  node->GetParentNode(getter_AddRefs(parent));
  if (parent) {
    nsCOMPtr<nsIXFormsUIWidget> widget(do_QueryInterface(parent));
    if (widget) {
      PRBool isFocused;
      widget->Focus(&isFocused);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::GetValue(nsAString& aValue)
{
  // The order of precedence for determining the label is:
  //   single node binding, linking, inline text (8.3.3)

  nsXFormsDelegateStub::GetValue(aValue);
  if (aValue.IsVoid() && !mSrcAttrText.IsEmpty()) {
    // handle linking ('src') attribute
    aValue = NS_ConvertUTF8toUTF16(mSrcAttrText);
  }
  if (aValue.IsVoid()) {
    NS_ENSURE_STATE(mElement);

    nsCOMPtr<nsIDOM3Node> inner(do_QueryInterface(mElement));
    if (inner) {
      inner->GetTextContent(aValue);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::WidgetAttached()
{
  mWidgetLoaded = PR_TRUE;

  // We shouldn't report to model that label is ready to work
  // if label is loading external resource. We will report
  // about it later when external resource will be loaded.
  if (!mChannel)
    nsXFormsDelegateStub::WidgetAttached();

  return NS_OK;
}

void
nsXFormsLabelElement::LoadExternalLabel(const nsAString& aSrc)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (doc) {
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aSrc, doc->GetDocumentCharacterSet().get(),
              doc->GetDocumentURI());
    if (uri) {
      if (nsXFormsUtils::CheckConnectionAllowed(mElement, uri)) {
        nsCOMPtr<nsILoadGroup> loadGroup;
        loadGroup = doc->GetDocumentLoadGroup();
        NS_WARN_IF_FALSE(loadGroup, "No load group!");

        // Using the same load group as the main document and creating
        // the channel with LOAD_NORMAL flag delays the dispatching of
        // the 'load' event until label data document has been loaded.
        NS_NewChannel(getter_AddRefs(mChannel), uri, nsnull, loadGroup,
                      this, nsIRequest::LOAD_NORMAL);

        if (mChannel) {
          rv = mChannel->AsyncOpen(this, nsnull);
          if (NS_FAILED(rv)) {
            // URI doesn't exist; report error.
            mChannel = nsnull;

            const nsPromiseFlatString& flat = PromiseFlatString(aSrc);
            const PRUnichar *strings[] = { flat.get(),
                                           NS_LITERAL_STRING("label").get() };
            nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLink1Error"),
                                       strings, 2, mElement, mElement);

            nsCOMPtr<nsIModelElementPrivate> modelPriv =
                                              nsXFormsUtils::GetModel(mElement);
            nsCOMPtr<nsIDOMNode> model = do_QueryInterface(modelPriv);
            nsXFormsUtils::DispatchEvent(model, eEvent_LinkError, nsnull,
                                         mElement);
          }
        }
      } else {
        const PRUnichar *strings[] = { NS_LITERAL_STRING("label").get() };
        nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLinkLoadOrigin"),
                                   strings, 1, mElement, mElement);
        nsCOMPtr<nsIModelElementPrivate> modelPriv =
          nsXFormsUtils::GetModel(mElement);
        nsCOMPtr<nsIDOMNode> model = do_QueryInterface(modelPriv);
        nsXFormsUtils::DispatchEvent(model, eEvent_LinkError, nsnull, mElement);
      }
    }
  }
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsLabelElement::Refresh()
{
  nsresult rv = nsXFormsDelegateStub::Refresh();
  if (NS_FAILED(rv) || rv == NS_OK_XFORMS_NOREFRESH)
    return rv;

  nsCOMPtr<nsIDOMNode> parent;
  mElement->GetParentNode(getter_AddRefs(parent));

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::IsEventTarget(PRBool *aOK)
{
  *aOK = PR_FALSE;
  return NS_OK;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
nsXFormsLabelElement::GetInterface(const nsIID &aIID, void **aResult)
{
  *aResult = nsnull;
  return QueryInterface(aIID, aResult);
}

// nsIStreamListener

// It is possible that the label element could well on its way to invalid by
// the time that the below handlers are called.  If the document hit a
// parsing error after we've already started to load the external source, then
// Mozilla will destroy all of the elements and the document in order to load
// the parser error page.  This will cause mElement to become null but since
// the channel will hold a nsCOMPtr to the nsXFormsLabelElement preventing
// it from being freed up, the channel will still be able to call the
// nsIStreamListener functions that we implement here.  And calling
// mChannel->Cancel() is no guarantee that these other notifications won't come
// through if the timing is wrong.  So we need to check for mElement below
// before we handle any of the stream notifications.

NS_IMETHODIMP
nsXFormsLabelElement::OnStartRequest(nsIRequest *aRequest,
                                     nsISupports *aContext)
{
  if (!mElement) {
    return NS_OK;
  }

  // Only handle data from text files for now.  Cancel any other requests.
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel) {
    nsCAutoString type;
    channel->GetContentType(type);
    if (!type.EqualsLiteral("text/plain"))
      return NS_ERROR_ILLEGAL_VALUE;
  }

  mSrcAttrText.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::OnDataAvailable(nsIRequest *aRequest,
                                      nsISupports *aContext,
                                      nsIInputStream *aInputStream,
                                      PRUint32 aOffset,
                                      PRUint32 aCount)
{
  if (!mElement) {
    return NS_OK;
  }

  nsresult rv;
  PRUint32 size, bytesRead;
  char buffer[256];

  while (aCount) {
    size = PR_MIN(aCount, sizeof(buffer));
    rv = aInputStream->Read(buffer, size, &bytesRead);
    if (NS_FAILED(rv))
      return rv;
    mSrcAttrText.Append(buffer, bytesRead);
    aCount -= bytesRead;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLabelElement::OnStopRequest(nsIRequest *aRequest,
                                    nsISupports *aContext,
                                    nsresult aStatusCode)
{
  // Done with load request, so null out channel member
  mChannel = nsnull;
  if (!mElement) {
    return NS_OK;
  }

  if (NS_FAILED(aStatusCode)) {
    // If we received NS_BINDING_ABORTED, then we were cancelled by a later
    // AttributeSet() or AttributeRemoved call.  Don't do anything and return.
    if (aStatusCode == NS_BINDING_ABORTED)
      return NS_OK;

    nsAutoString src;
    mElement->GetAttribute(NS_LITERAL_STRING("src"), src);
    const PRUnichar *strings[] = { NS_LITERAL_STRING("label").get(), 
                                   src.get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("externalLink2Error"),
                               strings, 2, mElement, mElement);

    nsCOMPtr<nsIModelElementPrivate> modelPriv =
      nsXFormsUtils::GetModel(mElement);
    nsCOMPtr<nsIDOMNode> model = do_QueryInterface(modelPriv);
    nsXFormsUtils::DispatchEvent(model, eEvent_LinkError, nsnull, mElement);

    mSrcAttrText.Truncate();
  }

  if (mWidgetLoaded)
    nsXFormsDelegateStub::WidgetAttached();

  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsLabelElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsLabelElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
