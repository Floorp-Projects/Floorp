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
 *  Darin Fisher <darin@meer.net>
 *  Doron Rosenberg <doronr@us.ibm.com>
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

#ifdef DEBUG_darinf
#include <stdio.h>
#define LOG(args) printf args
#else
#define LOG(args)
#endif

#include <stdlib.h>

#include "nsXFormsSubmissionElement.h"
#include "nsXFormsAtoms.h"
#include "nsIInstanceElementPrivate.h"
#include "nsIXTFElementWrapper.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMText.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMSerializer.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMParser.h"
#include "nsIAttribute.h"
#include "nsComponentManagerUtils.h"
#include "nsStringStream.h"
#include "nsIDocShell.h"
#include "nsIInputStream.h"
#include "nsIStorageStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsIMIMEInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIFileURL.h"
#include "nsIMIMEService.h"
#include "nsIUploadChannel.h"
#include "nsIHttpChannel.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPipe.h"
#include "nsLinebreakConverter.h"
#include "nsEscape.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsXFormsUtils.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIPermissionManager.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIMIMEHeaderParam.h"
#include "nsIExternalProtocolService.h"
#include "nsEscape.h"
#include "nsAutoPtr.h"

// namespace literals
#define kXMLNSNameSpaceURI \
        NS_LITERAL_STRING("http://www.w3.org/2000/xmlns/")

#define kIncludeNamespacePrefixes \
        NS_LITERAL_STRING("includenamespaceprefixes")

// submission methods
#define METHOD_GET                    0x01
#define METHOD_POST                   0x02
#define METHOD_PUT                    0x04

// submission encodings
#define ENCODING_XML                  0x10    // application/xml
#define ENCODING_URL                  0x20    // application/x-www-form-urlencoded
#define ENCODING_MULTIPART_RELATED    0x40    // multipart/related
#define ENCODING_MULTIPART_FORM_DATA  0x80    // multipart/form-data

struct SubmissionFormat
{
  const char *method;
  PRUint32    format;
};

static const SubmissionFormat sSubmissionFormats[] = {
  { "post",            ENCODING_XML                 | METHOD_POST },
  { "get",             ENCODING_URL                 | METHOD_GET  },
  { "put",             ENCODING_XML                 | METHOD_PUT  },
  { "multipart-post",  ENCODING_MULTIPART_RELATED   | METHOD_POST },
  { "form-data-post",  ENCODING_MULTIPART_FORM_DATA | METHOD_POST },
  { "urlencoded-post", ENCODING_URL                 | METHOD_POST }
};

static PRUint32
GetSubmissionFormat(nsIDOMElement *aElement)
{
  nsAutoString method;
  aElement->GetAttribute(NS_LITERAL_STRING("method"), method);

  NS_ConvertUTF16toUTF8 utf8method(method);
  for (PRUint32 i=0; i<NS_ARRAY_LENGTH(sSubmissionFormats); ++i)
  {
    // XXX case sensitive compare ok?
    if (utf8method.Equals(sSubmissionFormats[i].method))
      return sSubmissionFormats[i].format;
  }
  return 0;
}

#define ELEMENT_ENCTYPE_STRING 0
#define ELEMENT_ENCTYPE_URI    1
#define ELEMENT_ENCTYPE_BASE64 2
#define ELEMENT_ENCTYPE_HEX    3

static void
MakeMultipartBoundary(nsCString &boundary)
{
  boundary.AssignLiteral("---------------------------");
  boundary.AppendInt(rand());
  boundary.AppendInt(rand());
  boundary.AppendInt(rand());
}

static void
MakeMultipartContentID(nsCString &cid)
{
  cid.AppendInt(rand(), 16);
  cid.Append('.');
  cid.AppendInt(rand(), 16);
  cid.AppendLiteral("@mozilla.org");
}

static nsresult
URLEncode(const nsString &buf, nsCString &result)
{
  // 1. convert to UTF-8
  // 2. normalize newlines to \r\n
  // 3. escape, converting ' ' to '+'

  NS_ConvertUTF16toUTF8 utf8Buf(buf);

  char *convertedBuf =
      nsLinebreakConverter::ConvertLineBreaks(utf8Buf.get(),
                                              nsLinebreakConverter::eLinebreakAny,
                                              nsLinebreakConverter::eLinebreakNet);
  NS_ENSURE_TRUE(convertedBuf, NS_ERROR_OUT_OF_MEMORY);

  char *escapedBuf = nsEscape(convertedBuf, url_XPAlphas);
  nsMemory::Free(convertedBuf);

  NS_ENSURE_TRUE(escapedBuf, NS_ERROR_OUT_OF_MEMORY);

  result.Adopt(escapedBuf);
  return NS_OK;
}

static void
GetMimeTypeFromFile(nsIFile *file, nsCString &result)
{
  nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1");
  if (mime)
    mime->GetTypeFromFile(file, result);
  if (result.IsEmpty())
    result.Assign("application/octet-stream");
}

static PRBool
HasToken(const nsString &aTokenList, const nsString &aToken)
{
  PRInt32 i = aTokenList.Find(aToken);
  if (i == kNotFound)
    return PR_FALSE;
  
  // else, check that leading and trailing characters are either
  // not present or whitespace (#x20, #x9, #xD or #xA).

  if (i > 0)
  {
    PRUnichar c = aTokenList[i - 1];
    if (!(c == 0x20 || c == 0x9 || c == 0xD || c == 0xA))
      return PR_FALSE;
  }

  if (i + aToken.Length() < aTokenList.Length())
  {
    PRUnichar c = aTokenList[i + aToken.Length()];
    if (!(c == 0x20 || c == 0x9 || c == 0xD || c == 0xA))
      return PR_FALSE;
  }

  return PR_TRUE;
}

// structure used to store information needed to generate attachments
// for multipart/related submission.
struct SubmissionAttachment
{
  nsCOMPtr<nsIFile> file;
  nsCString         cid;
};

// an array of SubmissionAttachment objects
class SubmissionAttachmentArray : nsVoidArray
{
public:
  SubmissionAttachmentArray() {}
 ~SubmissionAttachmentArray()
  {
    for (PRUint32 i=0; i<Count(); ++i)
      delete (SubmissionAttachment *) ElementAt(i);
  }
  nsresult Append(nsIFile *file, const nsCString &cid)
  {
    SubmissionAttachment *a = new SubmissionAttachment;
    if (!a)
      return NS_ERROR_OUT_OF_MEMORY;
    a->file = file;
    a->cid = cid;
    AppendElement(a);
    return NS_OK;
  }
  PRUint32 Count() const
  {
    return (PRUint32) nsVoidArray::Count();
  }
  SubmissionAttachment *Item(PRUint32 index)
  {
    return (SubmissionAttachment *) ElementAt(index);
  }
};

// nsISupports

NS_IMPL_ISUPPORTS_INHERITED4(nsXFormsSubmissionElement,
                             nsXFormsStubElement,
                             nsIRequestObserver,
                             nsIXFormsSubmissionElement,
                             nsIInterfaceRequestor,
                             nsIChannelEventSink)

// nsIXTFElement

NS_IMETHODIMP
nsXFormsSubmissionElement::OnDestroyed()
{
  mElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  if (!nsXFormsUtils::EventHandlingAllowed(aEvent, mElement))
    return NS_OK;

  nsAutoString type;
  aEvent->GetType(type);
  if (type.EqualsLiteral("xforms-submit")) {
    // If the submission is already active, do nothing.
    if (!mSubmissionActive && NS_FAILED(Submit())) {
      EndSubmit(PR_FALSE);
    }

    *aHandled = PR_TRUE;
  } else {
    *aHandled = PR_FALSE;
  }

  return NS_OK;
}

// nsIXFormsSubmissionElement

NS_IMETHODIMP
nsXFormsSubmissionElement::SetActivator(nsIXFormsSubmitElement* aActivator)
{
  if (!mActivator && !mSubmissionActive)
    mActivator = aActivator;
  return NS_OK;
}

// nsIXTFElement

NS_IMETHODIMP
nsXFormsSubmissionElement::OnCreated(nsIXTFElementWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_HANDLE_DEFAULT);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  return NS_OK;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
nsXFormsSubmissionElement::GetInterface(const nsIID & aIID, void **aResult)
{
  *aResult = nsnull;
  return QueryInterface(aIID, aResult);
}

// nsIChannelEventSink

// It is possible that the submission element could well on its way to invalid
// by the time that the below handlers are called.  If the document was
// destroyed after we've already started submitting data then this will cause
// mElement to become null.  Since the channel will hold a nsCOMPtr
// to the nsXFormsSubmissionElement as a callback to the channel, this prevents
// it from being freed up.  The channel will still be able to call the
// nsIStreamListener functions that we implement here.  And calling
// mChannel->Cancel() is no guarantee that these other notifications won't come
// through if the timing is wrong.  So we need to check for mElement below
// before we handle any of the stream notifications.


NS_IMETHODIMP
nsXFormsSubmissionElement::OnChannelRedirect(nsIChannel *aOldChannel,
                                             nsIChannel *aNewChannel,
                                             PRUint32    aFlags)
{
  if (!mElement) {
    return NS_OK;
  }

  NS_PRECONDITION(aNewChannel, "Redirect without a channel?");
  nsCOMPtr<nsIURI> newURI;
  nsresult rv = aNewChannel->GetURI(getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_STATE(mElement);
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  NS_ENSURE_STATE(doc);

  if (!CheckSameOrigin(doc, newURI)) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("submitSendOrigin"),
                               mElement);
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::OnStartRequest(nsIRequest  *aRequest,
                                          nsISupports *aCtx)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::OnStopRequest(nsIRequest  *aRequest,
                                         nsISupports *aCtx,
                                         nsresult     aStatus)
{
  LOG(("xforms submission complete [status=%x]\n", aStatus));

  if (!mElement) {
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  NS_ASSERTION(channel, "request should be a channel");

  PRBool succeeded = NS_SUCCEEDED(aStatus);
  if (succeeded) {

    // If it is a HTTP request, then check for error responses, which should
    // result in NOP and an xforms-submit-error.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
    if (httpChannel) {
      PRUint32 response;
      nsresult rv = httpChannel->GetResponseStatus(&response);
      succeeded = NS_SUCCEEDED(rv) && (response < 400);
    }

    if (succeeded) {
      PRUint32 avail = 0;
      mPipeIn->Available(&avail);
      if (avail > 0) {

        nsresult rv;
        if (mIsReplaceInstance) {
          rv = LoadReplaceInstance(channel);
        } else {
          nsAutoString replace;
          mElement->GetAttribute(NS_LITERAL_STRING("replace"), replace);
          if (replace.IsEmpty() || replace.EqualsLiteral("all"))
            rv = LoadReplaceAll(channel);
          else
            rv = NS_OK;
        }
        succeeded = NS_SUCCEEDED(rv);
      }
    }
  }

  mPipeIn = 0;

  EndSubmit(succeeded);

  return NS_OK;
}

// private methods

void
nsXFormsSubmissionElement::EndSubmit(PRBool aSucceeded)
{
  mSubmissionActive = PR_FALSE;
  if (mActivator) {
    mActivator->SetDisabled(PR_FALSE);
    mActivator = nsnull;
  }
  nsXFormsUtils::DispatchEvent(mElement, aSucceeded ?
                               eEvent_SubmitDone : eEvent_SubmitError);
}

already_AddRefed<nsIModelElementPrivate>
nsXFormsSubmissionElement::GetModel()
{
  nsCOMPtr<nsIDOMNode> parentNode;
  mElement->GetParentNode(getter_AddRefs(parentNode));

  nsIModelElementPrivate *model = nsnull;
  if (parentNode)
    CallQueryInterface(parentNode, &model);
  return model;
}

nsresult
nsXFormsSubmissionElement::LoadReplaceInstance(nsIChannel *channel)
{
  NS_ASSERTION(channel, "LoadReplaceInstance called with null channel?");

  // replace instance document

  nsCString contentCharset;
  channel->GetContentCharset(contentCharset);

  // use DOM parser to construct nsIDOMDocument
  nsCOMPtr<nsIDOMParser> parser = do_CreateInstance("@mozilla.org/xmlextras/domparser;1");
  NS_ENSURE_STATE(parser);

  PRUint32 contentLength;
  mPipeIn->Available(&contentLength);

  // set the base uri so that the document can get the correct security
  // principal (this has to be here to work on 1.8.0)
  // @see https://bugzilla.mozilla.org/show_bug.cgi?id=338451
  nsCOMPtr<nsIURI> uri;
  nsresult rv = channel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  // XXXbz is this the right principal?
  rv = parser->Init(nsnull, uri, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> newDoc;
  parser->ParseFromStream(mPipeIn, contentCharset.get(), contentLength,
                          "application/xml", getter_AddRefs(newDoc));
  // XXX Add URI, etc?
  if (!newDoc) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("instanceParseError"),
                               mElement);
    return NS_ERROR_UNEXPECTED;
  }
  
  // check for parsererror tag?  XXX is this needed?  or, is there a better way?
  nsCOMPtr<nsIDOMElement> docElem;
  newDoc->GetDocumentElement(getter_AddRefs(docElem));
  if (docElem) {
    nsAutoString tagName, namespaceURI;
    docElem->GetTagName(tagName);
    docElem->GetNamespaceURI(namespaceURI);

    // XXX this is somewhat of a hack.  we should instead be listening for an
    // 'error' event from the DOM, but gecko doesn't implement that event yet.
    if (tagName.EqualsLiteral("parsererror") &&
        namespaceURI.EqualsLiteral("http://www.mozilla.org/newlayout/xml/parsererror.xml")) {
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("instanceParseError"),
                                 mElement);
      return NS_ERROR_UNEXPECTED;
    }
  }

  // Get the appropriate instance node.  If the "instance" attribute is set,
  // then get that instance node.  Otherwise, get the one we are bound to.
  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  NS_ENSURE_STATE(model);
  nsCOMPtr<nsIInstanceElementPrivate> instanceElement;
  nsAutoString value;
  mElement->GetAttribute(NS_LITERAL_STRING("instance"), value);
  if (!value.IsEmpty()) {
    rv = GetSelectedInstanceElement(value, model,
                                    getter_AddRefs(instanceElement));
  } else {
    nsCOMPtr<nsIDOMNode> data;
    rv = GetBoundInstanceData(getter_AddRefs(data));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIDOMNode> instanceNode;
      rv = nsXFormsUtils::GetInstanceNodeForData(data,
                                                 getter_AddRefs(instanceNode));
      NS_ENSURE_SUCCESS(rv, rv);

      instanceElement = do_QueryInterface(instanceNode);
    }
  }

  // replace the document referenced by this instance element with the info
  // returned back from the submission
  if (NS_SUCCEEDED(rv) && instanceElement) {
    instanceElement->SetInstanceDocument(newDoc);

    // refresh everything
    model->Rebuild();
    model->Recalculate();
    model->Revalidate();
    model->Refresh();
  }


  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::GetSelectedInstanceElement(
                                            const nsAString &aInstanceID,
                                            nsIModelElementPrivate *aModel,
                                            nsIInstanceElementPrivate **aResult)
{
  aModel->FindInstanceElement(aInstanceID, aResult);
  if (*aResult == nsnull) {
    // if failed to get desired instance, dispatch binding exception
    const PRUnichar *strings[] = { PromiseFlatString(aInstanceID).get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("instanceBindError"),
                               strings, 1, mElement, mElement);
    nsXFormsUtils::DispatchEvent(mElement, eEvent_BindingException);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::LoadReplaceAll(nsIChannel *channel)
{
  // use nsIDocShell::loadStream, which may not be perfect ;-)

  // XXX do we need to transfer nsIChannel::securityInfo ???

  nsCOMPtr<nsIContent> content(do_QueryInterface(mElement));
  NS_ASSERTION(content, "mElement not implementing nsIContent?!");
  nsIDocument* doc = content->GetCurrentDoc();
  NS_ENSURE_STATE(doc);

  // the container is the docshell, and we use it as our provider of
  // notification callbacks.
  nsCOMPtr<nsISupports> container = doc->GetContainer();
  nsCOMPtr<nsIDocShell> docshell = do_QueryInterface(container);

  nsCOMPtr<nsIURI> uri;
  nsCString contentType, contentCharset;

  channel->GetURI(getter_AddRefs(uri));
  channel->GetContentType(contentType);
  channel->GetContentCharset(contentCharset);

  return docshell->LoadStream(mPipeIn, uri, contentType, contentCharset, nsnull);
}

nsresult
nsXFormsSubmissionElement::Submit()
{
  LOG(("+++ nsXFormsSubmissionElement::Submit\n"));

  NS_ENSURE_STATE(mElement);

  nsresult rv;
  mIsSOAPRequest = PR_FALSE;

  //
  // 1. ensure that we are not currently processing a xforms-submit (see E37)
  if (mSubmissionActive) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("warnSubmitAlreadyRunning"),
                               mElement, nsIScriptError::warningFlag);
    return NS_ERROR_FAILURE;
  }
  mSubmissionActive = PR_TRUE;
  
  if (mActivator)
    mActivator->SetDisabled(PR_TRUE);

  // Someone may change the "replace" attribute during submission
  // and that would break the ::SameOriginCheck().
  nsAutoString replace;
  mElement->GetAttribute(NS_LITERAL_STRING("replace"), replace);
  mIsReplaceInstance = replace.EqualsLiteral("instance");

  //
  // 2. get selected node from the instance data
  nsCOMPtr<nsIDOMNode> data;
  rv = GetBoundInstanceData(getter_AddRefs(data));
  NS_ENSURE_SUCCESS(rv, rv);

  // No data to submit
  if (!data) {
    EndSubmit(PR_FALSE);
    return NS_OK;
  }

  //
  // 3. Create submission document (include namespaces, purge non-relevant
  // nodes, check simple type validity)
  nsCOMPtr<nsIDOMDocument> submissionDoc;
  rv = CreateSubmissionDoc(data, getter_AddRefs(submissionDoc));
  NS_ENSURE_SUCCESS(rv, rv);

  //
  // 4. Validate document
  // XXX: model->ValidateDocument(submissionDoc, &res);

  //
  // 5. Convert submission document into the requested format
  // Checking the format only before starting the submission.
  mFormat = GetSubmissionFormat(mElement);
  NS_ENSURE_STATE(mFormat != 0);

  nsCOMPtr<nsIInputStream> stream;
  nsCAutoString uri, contentType;
  nsAutoString action;
  mElement->GetAttribute(NS_LITERAL_STRING("action"), action);
  CopyUTF16toUTF8(action, uri);

  rv = SerializeData(submissionDoc, uri, getter_AddRefs(stream), contentType);
  if (NS_FAILED(rv)) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("warnSubmitSerializeFailed"),
                               mElement, nsIScriptError::warningFlag);
    return rv;
  }

  //
  // 6. dispatch network request
  rv = SendData(uri, stream, contentType);
  if (NS_FAILED(rv)) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("warnSubmitNetworkFailure"),
                               mElement, nsIScriptError::warningFlag);
    return rv;
  }

  return rv;
}

nsresult
nsXFormsSubmissionElement::GetBoundInstanceData(nsIDOMNode **result)
{
  nsCOMPtr<nsIModelElementPrivate> model;
  nsCOMPtr<nsIDOMXPathResult> xpRes;
  PRBool usesModelBind;
  nsresult rv =
    nsXFormsUtils::EvaluateNodeBinding(mElement, 0,
                                       NS_LITERAL_STRING("ref"),
                                       NS_LITERAL_STRING("/"),
                                       nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                       getter_AddRefs(model),
                                       getter_AddRefs(xpRes),
                                       &usesModelBind);

  if (NS_FAILED(rv) || !xpRes)
    return NS_ERROR_UNEXPECTED;

  return usesModelBind ? xpRes->SnapshotItem(0, result)
                       : xpRes->GetSingleNodeValue(result);
}

PRBool
nsXFormsSubmissionElement::GetBooleanAttr(const nsAString &name,
                                          PRBool defaultVal)
{
  nsAutoString value;
  mElement->GetAttribute(name, value);

  // use defaultVal when value does not match a legal literal

  if (!value.IsEmpty())
  {
    if (value.EqualsLiteral("true") || value.EqualsLiteral("1"))
      return PR_TRUE;
    if (value.EqualsLiteral("false") || value.EqualsLiteral("0"))
      return PR_FALSE;
  }
  
  return defaultVal;
}

void
nsXFormsSubmissionElement::GetDefaultInstanceData(nsIDOMNode **result)
{
  *result = nsnull;

  // default <instance> element is the first <instance> child node of 
  // our parent, which should be a <model> element.

  nsCOMPtr<nsIDOMNode> parent;
  mElement->GetParentNode(getter_AddRefs(parent));
  if (!parent)
  {
    NS_WARNING("no parent node!");
    return;
  }

  nsCOMPtr<nsIXFormsModelElement> model = do_QueryInterface(parent);
  if (!model)
  {
    NS_WARNING("parent node is not a model");
    return;
  }

  nsCOMPtr<nsIDOMDocument> instanceDoc;
  model->GetInstanceDocument(EmptyString(), getter_AddRefs(instanceDoc));

  nsCOMPtr<nsIDOMElement> instanceDocElem;
  instanceDoc->GetDocumentElement(getter_AddRefs(instanceDocElem));

  NS_ADDREF(*result = instanceDocElem);
}

nsresult
nsXFormsSubmissionElement::SerializeData(nsIDOMDocument  *aData,
                                         nsCString       &aUri,
                                         nsIInputStream **aStream,
                                         nsCString       &aContentType)
{
  if (mFormat & ENCODING_XML)
    return SerializeDataXML(aData, aStream, aContentType);

  if (mFormat & ENCODING_URL)
    return SerializeDataURLEncoded(aData, aUri, aStream, aContentType);

  if (mFormat & ENCODING_MULTIPART_RELATED)
    return SerializeDataMultipartRelated(aData, aStream, aContentType);

  if (mFormat & ENCODING_MULTIPART_FORM_DATA)
    return SerializeDataMultipartFormData(aData, aStream, aContentType);

  NS_WARNING("unsupported submission encoding");
  return NS_ERROR_UNEXPECTED;
}

nsresult
nsXFormsSubmissionElement::SerializeDataXML(nsIDOMDocument  *data,
                                            nsIInputStream **stream,
                                            nsCString        &contentType)
{
  nsresult rv;
  nsAutoString mediaType;
  mElement->GetAttribute(NS_LITERAL_STRING("mediatype"), mediaType);

  // Check for preference, disabling SOAP requests
  if (nsXFormsUtils::ExperimentalFeaturesEnabled()) {

    // Check for SOAP Envelope and handle SOAP
    nsAutoString nodeName, nodeNS;
    data->GetLocalName(nodeName);
    data->GetNamespaceURI(nodeNS);
    if (nodeName.Equals(NS_LITERAL_STRING("Envelope")) &&
        nodeNS.Equals(NS_LITERAL_STRING(NS_NAMESPACE_SOAP_ENVELOPE))) {
      mIsSOAPRequest = PR_TRUE;
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("warnSOAP"), mElement,
                                 nsIScriptError::warningFlag);
      contentType.AssignLiteral("text/xml");

      if (!mediaType.IsEmpty()) {
        // copy charset from mediatype
        nsAutoString charset;
        nsCOMPtr<nsIMIMEHeaderParam> mimeHdrParser =
          do_GetService("@mozilla.org/network/mime-hdrparam;1");
        NS_ENSURE_STATE(mimeHdrParser);
        rv = mimeHdrParser->GetParameter(NS_ConvertUTF16toUTF8(mediaType),
                                         "charset", EmptyCString(), PR_FALSE,
                                         nsnull, charset);
        if (NS_SUCCEEDED(rv) && !charset.IsEmpty()) {
          contentType.AppendLiteral("; charset=");
          contentType.Append(NS_ConvertUTF16toUTF8(charset));
        }
      }
    }
  }

  // Handle non-SOAP requests
  if (!mIsSOAPRequest) {
    if (mediaType.IsEmpty())
      contentType.AssignLiteral("application/xml");
    else
      CopyUTF16toUTF8(mediaType, contentType);
  }
  
  nsCOMPtr<nsIStorageStream> storage;
  NS_NewStorageStream(4096, PR_UINT32_MAX, getter_AddRefs(storage));
  NS_ENSURE_TRUE(storage, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIOutputStream> sink;
  storage->GetOutputStream(0, getter_AddRefs(sink));
  NS_ENSURE_TRUE(sink, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIDOMSerializer> serializer =
      do_GetService("@mozilla.org/xmlextras/xmlserializer;1");
  NS_ENSURE_STATE(serializer);

  // Serialize content
  nsAutoString encoding;
  mElement->GetAttribute(NS_LITERAL_STRING("encoding"), encoding);
  if (encoding.IsEmpty())
    encoding.AssignLiteral("UTF-8");

  // XXX: should check @indent and possibly indent content. Bug 278761
  rv = serializer->SerializeToStream(data, sink,
                                     NS_LossyConvertUTF16toASCII(encoding));
  NS_ENSURE_SUCCESS(rv, rv);

  // close the output stream, so that the input stream will not return
  // NS_BASE_STREAM_WOULD_BLOCK when it reaches end-of-stream.
  sink->Close();

  return storage->NewInputStream(0, stream);
}

PRBool
nsXFormsSubmissionElement::CheckSameOrigin(nsIDocument *aBaseDocument,
                                           nsIURI      *aTestURI)
{
  // we default to true to allow regular posts to work like html forms.
  PRBool allowSubmission = PR_TRUE;

  /* for replace="instance" or XML submission, we follow these strict guidelines:
       - we default to denying submission
       - if we are not replacing instance, then file:// urls can submit anywhere.
         We don't allow fetching of content for file:// urls since for example
         XMLHttpRequest doesn't, since file:// doesn't always mean it is local.
       - if we are still denying, we check the permission manager to see if the
         domain hosting the XForm has been granted permission to get/send data
         anywhere
       - lastly, if submission is still being denied, we do a same origin check
   */
  if (mFormat & (ENCODING_XML | ENCODING_MULTIPART_RELATED) || mIsReplaceInstance) {

    // if same origin is required, default to false
    allowSubmission = PR_FALSE;
    nsIURI *baseURI = aBaseDocument->GetDocumentURI();

    // if we don't replace the instance, we allow file:// to submit data anywhere
    if (!mIsReplaceInstance) {
      baseURI->SchemeIs("file", &allowSubmission);
    }

    // if none of the above checks have allowed the submission, we do a
    // same origin check.
    if (!allowSubmission) {
      // replace instance is both a send and a load
      nsXFormsUtils::ConnectionType mode;
      if (mIsReplaceInstance)
        mode = nsXFormsUtils::kXFormsActionLoadSend;
      else
        mode = nsXFormsUtils::kXFormsActionSend;

      allowSubmission =
        nsXFormsUtils::CheckConnectionAllowed(mElement, aTestURI, mode);
    }
  }

  return allowSubmission;
}

nsresult
nsXFormsSubmissionElement::AddNameSpaces(nsIDOMElement   *aTarget,
                                         nsIDOMNode      *aSource,
                                         nsStringHashSet *aPrefixHash)
{
  nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
  nsCOMPtr<nsIDOMNode> attrNode;
  nsAutoString nsURI, localName, value;

  aSource->GetAttributes(getter_AddRefs(attrMap));
  NS_ENSURE_STATE(attrMap);

  PRUint32 length;
  attrMap->GetLength(&length);

  for (PRUint32 run = 0; run < length; ++run) {
    attrMap->Item(run, getter_AddRefs(attrNode));
    attrNode->GetNamespaceURI(nsURI);

    if (nsURI.Equals(kXMLNSNameSpaceURI)) {
      attrNode->GetLocalName(localName);
      attrNode->GetNodeValue(value);

      if (!localName.EqualsLiteral("xmlns")) {
        if (!aPrefixHash || aPrefixHash->Contains(localName)) {
          nsAutoString attrName(NS_LITERAL_STRING("xmlns:"));
          attrName.Append(localName);
          aTarget->SetAttributeNS(kXMLNSNameSpaceURI, attrName, value);
        }
      } else if (!value.IsEmpty()) {
        PRBool hasDefaultNSAttr;
        aTarget->HasAttributeNS(kXMLNSNameSpaceURI,
                                NS_LITERAL_STRING("xmlns"), &hasDefaultNSAttr);

        if (!hasDefaultNSAttr) {
          aTarget->GetNamespaceURI(nsURI);
          if (!nsURI.IsEmpty()) {
            // if aTarget default namespace uri isn't empty and it hasn't
            // default namespace attribute then we should add it.
            aTarget->SetAttributeNS(kXMLNSNameSpaceURI, localName, value);
          }
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::GetIncludeNSPrefixesAttr(nsStringHashSet** aHash)
{
  NS_PRECONDITION(aHash, "null ptr");
  if (!aHash)
    return NS_ERROR_NULL_POINTER;

  *aHash = new nsStringHashSet();
  if (!*aHash)
    return NS_ERROR_OUT_OF_MEMORY;
  (*aHash)->Init(5);

  nsAutoString prefixes;
  mElement->GetAttribute(kIncludeNamespacePrefixes, prefixes);

  // Cycle through space-delimited list and populate hash set
  if (!prefixes.IsEmpty()) {
    PRInt32 start = 0, end;
    PRInt32 length = prefixes.Length();

    do {
      end = prefixes.FindCharInSet(" \t\r\n", start);
      if (end != kNotFound) {
        if (start != end) {   // this line handles consecutive space chars
          const nsAString& p = Substring(prefixes, start, end - start);
          (*aHash)->Put(p);
        }
        start = end + 1;
      }
    } while (end != kNotFound && start != length);

    if (start != length) {
      const nsAString& p = Substring(prefixes, start);
      (*aHash)->Put(p);
    }
  }

  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::CreateSubmissionDoc(nsIDOMNode      *aRoot,
                                               nsIDOMDocument **aReturnDoc)
{
  NS_ENSURE_ARG_POINTER(aRoot);
  NS_ENSURE_ARG_POINTER(aReturnDoc);

  nsCOMPtr<nsIDOMDocument> instDoc, submDoc;
  aRoot->GetOwnerDocument(getter_AddRefs(instDoc));
  nsresult rv;

  if (!instDoc) {
    // owner doc is null when the aRoot node is the document (e.g., ref="/")
    // so we can just get the document via QI.
    instDoc = do_QueryInterface(aRoot);
    NS_ENSURE_STATE(instDoc);

    rv = CreatePurgedDoc(instDoc, getter_AddRefs(submDoc));
  } else {
    rv = CreatePurgedDoc(aRoot, getter_AddRefs(submDoc));
  }
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(submDoc);

  // We now need to add namespaces to the submission document.  We get them
  // from 3 sources - the main document's documentElement, the model and the
  // xforms:instance that contains the submitted instance data node.

  nsCOMPtr<nsIDOMNode> instanceNode;
  rv = nsXFormsUtils::GetInstanceNodeForData(aRoot,
                                             getter_AddRefs(instanceNode));
  NS_ENSURE_SUCCESS(rv, rv);

  // add namespaces from the main document to the submission document, but only
  // if the instance data is local, not remote.
  PRBool serialize = PR_FALSE;
  nsCOMPtr<nsIDOMElement> instanceElement(do_QueryInterface(instanceNode));

  // make sure that this is a DOMElement.  It won't be if it was lazy
  // authored.  Lazy authored instance documents don't inherit namespaces
  // from parent nodes or the original document (in formsPlayer and Novell,
  // at least).
  if (instanceElement) {
    PRBool hasSrc = PR_FALSE;
    instanceElement->HasAttribute(NS_LITERAL_STRING("src"), &hasSrc);
    serialize = !hasSrc;
  }

  if (serialize) {
    // Handle "includenamespaceprefixes" attribute, if present
    nsAutoPtr<nsStringHashSet> prefixHash;
    PRBool hasPrefixAttr = PR_FALSE;
    mElement->HasAttribute(kIncludeNamespacePrefixes, &hasPrefixAttr);
    if (hasPrefixAttr) {
      rv = GetIncludeNSPrefixesAttr(getter_Transfers(prefixHash));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // get the document element of the document we are going to submit
    nsCOMPtr<nsIDOMElement> submDocElm;
    submDoc->GetDocumentElement(getter_AddRefs(submDocElm));
    NS_ENSURE_STATE(submDocElm);

    // if submission document has empty default namespace attribute and if
    // @includenamespaceprefixes attribute doesn't contain "#default" value then
    // we should remove default namespace attribute (see the specs 11.3).
    nsAutoString XMLNSAttrValue;
    submDocElm->GetAttributeNS(kXMLNSNameSpaceURI, NS_LITERAL_STRING("xmlns"),
                               XMLNSAttrValue);

    if (XMLNSAttrValue.IsEmpty() && (!prefixHash ||
        !prefixHash->Contains(NS_LITERAL_STRING("#default")))) {
      submDocElm->RemoveAttributeNS(kXMLNSNameSpaceURI,
                                    NS_LITERAL_STRING("xmlns"));
    }

    // handle namespaces on the root element of the instance document
    nsCOMPtr<nsIDOMElement> instDocElm;
    instDoc->GetDocumentElement(getter_AddRefs(instDocElm));
    nsCOMPtr<nsIDOMNode> instDocNode(do_QueryInterface(instDocElm));
    NS_ENSURE_STATE(instDocNode);
    rv = AddNameSpaces(submDocElm, instDocNode, prefixHash);
    NS_ENSURE_SUCCESS(rv, rv);

    // handle namespaces on the xforms:instance
    rv = AddNameSpaces(submDocElm, instanceNode, prefixHash);
    NS_ENSURE_SUCCESS(rv, rv);

    // handle namespaces on the model
    nsCOMPtr<nsIModelElementPrivate> model = GetModel();
    nsCOMPtr<nsIDOMNode> modelNode(do_QueryInterface(model));
    NS_ENSURE_STATE(modelNode);
    rv = AddNameSpaces(submDocElm, modelNode, prefixHash);
    NS_ENSURE_SUCCESS(rv, rv);

    // handle namespace on main document
    nsCOMPtr<nsIDOMDocument> mainDoc;
    mElement->GetOwnerDocument(getter_AddRefs(mainDoc));
    NS_ENSURE_STATE(mainDoc);

    nsCOMPtr<nsIDOMElement> mainDocElm;
    mainDoc->GetDocumentElement(getter_AddRefs(mainDocElm));
    nsCOMPtr<nsIDOMNode> mainDocNode(do_QueryInterface(mainDocElm));
    NS_ENSURE_STATE(mainDocNode);

    rv = AddNameSpaces(submDocElm, mainDocNode, prefixHash);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*aReturnDoc = submDoc);

  return NS_OK;
}


nsresult
nsXFormsSubmissionElement::CreatePurgedDoc(nsIDOMNode      *source,
                                           nsIDOMDocument **result)
{
  PRBool omit_xml_declaration
      = GetBooleanAttr(NS_LITERAL_STRING("omit-xml-declaration"), PR_FALSE);

  nsAutoString cdataElements;
  mElement->GetAttribute(NS_LITERAL_STRING("cdata-section-elements"),
                         cdataElements);

  // XXX cdataElements contains space delimited QNames.  these may have
  //     namespace prefixes relative to our document.  we need to translate
  //     them to the corresponding namespace prefix in the source document.
  //
  // XXX we'll just assume that the QNames correspond to node names since
  //     it's unclear how to resolve the namespace prefixes any other way.

  // source can be a document or just a node
  nsCOMPtr<nsIDOMDocument> sourceDoc(do_QueryInterface(source));
  nsCOMPtr<nsIDOMDOMImplementation> impl;

  if (sourceDoc) {
    sourceDoc->GetImplementation(getter_AddRefs(impl));
  } else {
    nsCOMPtr<nsIDOMDocument> tmpDoc;
    source->GetOwnerDocument(getter_AddRefs(tmpDoc));
    tmpDoc->GetImplementation(getter_AddRefs(impl));
  }
  NS_ENSURE_STATE(impl);

  nsCOMPtr<nsIDOMDocument> doc;
  impl->CreateDocument(EmptyString(), EmptyString(), nsnull,
                       getter_AddRefs(doc));
  NS_ENSURE_STATE(doc);

  if (!omit_xml_declaration) {
    nsAutoString encoding;
    mElement->GetAttribute(NS_LITERAL_STRING("encoding"), encoding);
    if (encoding.IsEmpty())
      encoding.AssignLiteral("UTF-8");

    nsAutoString buf =
      NS_LITERAL_STRING("version=\"1.0\" encoding=\"") +
      encoding +
      NS_LITERAL_STRING("\"");

    nsCOMPtr<nsIDOMProcessingInstruction> pi;
    doc->CreateProcessingInstruction(NS_LITERAL_STRING("xml"), buf,
                                     getter_AddRefs(pi));

    nsCOMPtr<nsIDOMNode> newChild;
    doc->AppendChild(pi, getter_AddRefs(newChild));
  }

  // recursively walk the source document, copying nodes as appropriate
  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  NS_ENSURE_STATE(model);
  nsresult rv = NS_OK;
  // if it is a document, get the root element
  if (sourceDoc) {
    // Iterate over document child nodes to preserve document level
    // processing instructions and comment nodes.
    nsCOMPtr<nsIDOMNode> curDocNode, node, destChild;
    sourceDoc->GetFirstChild(getter_AddRefs(curDocNode));
    PRUint16 type;
    while (curDocNode) {
      curDocNode->GetNodeType(&type);
      if (type == nsIDOMNode::ELEMENT_NODE) {
        rv = CopyChildren(model, curDocNode, doc, doc, cdataElements, 0);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        doc->ImportNode(curDocNode, PR_FALSE, getter_AddRefs(destChild));
        doc->AppendChild(destChild, getter_AddRefs(node));
      }

      curDocNode->GetNextSibling(getter_AddRefs(node));
      curDocNode.swap(node);
    }
  } else {
    rv = CopyChildren(model, source, doc, doc, cdataElements, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*result = doc);
  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::CreateAttachments(nsIModelElementPrivate *aModel,
                                             nsIDOMNode             *aNode,
                                             SubmissionAttachmentArray *aAttachments)
{
  nsCOMPtr<nsIDOMNode> currentNode(aNode);

  while (currentNode) {
    PRUint16 currentNodeType;
    nsresult rv = currentNode->GetNodeType(&currentNodeType);
    NS_ENSURE_SUCCESS(rv, rv);

    // If |currentNode| is an element node of type 'xsd:anyURI', we need to
    // generate a ContentID for the child of this element, and append a new
    // attachment to the attachments array.

    PRUint32 encType;
    if (NS_SUCCEEDED(GetElementEncodingType(currentNode, &encType, aModel)) &&
        encType == ELEMENT_ENCTYPE_URI) {
      // ok, looks like we have a local file to upload

      // uploadFileProperty can exist on attribute nodes if an upload is bound
      // to an attribute.  But we'll have to look for such attributes as we
      // we encounter the element nodes that contain them.  We won't reach
      // attributes walking the child/sibling chain of nodes.  So here just
      // test for nsIContent.
      void* uploadFileProperty = nsnull;
      nsCOMPtr<nsIContent> content = do_QueryInterface(currentNode);
      if (content) {
        uploadFileProperty =
          content->GetProperty(nsXFormsAtoms::uploadFileProperty);
      }

      nsIFile *file = NS_STATIC_CAST(nsIFile *, uploadFileProperty);
      // NOTE: this value may be null if a file hasn't been selected.

      if (uploadFileProperty) {
        nsCString cid;
        cid.AssignLiteral("cid:");
        MakeMultipartContentID(cid);
  
        nsCOMPtr<nsIDOMNode> childNode;
      
        switch (currentNodeType) {

        case nsIDOMNode::TEXT_NODE:
        case nsIDOMNode::CDATA_SECTION_NODE:
        case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
        case nsIDOMNode::COMMENT_NODE:
          rv = currentNode->SetNodeValue(NS_ConvertUTF8toUTF16(cid));
          NS_ENSURE_SUCCESS(rv, rv);
      
          break;
      
        case nsIDOMNode::ELEMENT_NODE:
      
          rv = currentNode->GetFirstChild(getter_AddRefs(childNode));
          NS_ENSURE_SUCCESS(rv, rv);
      
          // shouldn't have to worry about the case of there not being a child
          // node here.  If uploadFileProperty is set then that means that
          // the node that 'currentNode' was cloned from has has gone through
          // through model.SetNodeValue, so should already have a text node
          // as the first child and no extraneous text nodes
          // following the first one.  We'll check to make sure, though.
          PRUint16 childType;
          rv = childNode->GetNodeType(&childType);
          NS_ENSURE_SUCCESS(rv, rv);
      
          if (childType == nsIDOMNode::TEXT_NODE ||
              childType == nsIDOMNode::CDATA_SECTION_NODE) {
            rv = childNode->SetNodeValue(NS_ConvertUTF8toUTF16(cid));
            NS_ENSURE_SUCCESS(rv, rv);
          } else {
            return NS_ERROR_UNEXPECTED;
          }
        }
        aAttachments->Append(file, cid);
      }
    }

    // look to see if the element node has any attributes with an
    // uploadFileProperty on it.
    if (currentNodeType == nsIDOMNode::ELEMENT_NODE) {
      PRBool hasAttributes = PR_FALSE;
      currentNode->HasAttributes(&hasAttributes);
      if (hasAttributes) {
        nsCOMPtr<nsIDOMNamedNodeMap> attrs;
        currentNode->GetAttributes(getter_AddRefs(attrs));
        NS_ENSURE_STATE(attrs);
        PRUint32 length;
        attrs->GetLength(&length);
        nsCOMPtr<nsIDOMNode> attrDOMNode;
        for (PRUint32 i = 0; i < length; ++i) {
          attrs->Item(i, getter_AddRefs(attrDOMNode));
          NS_ENSURE_STATE(attrDOMNode);
          nsCOMPtr<nsIAttribute> attr = do_QueryInterface(attrDOMNode);
          NS_ENSURE_STATE(attr);
          void *uploadFileProperty =
            attr->GetProperty(nsXFormsAtoms::uploadFileProperty);
  
          if (!uploadFileProperty) {
            continue;
          }

          nsIFile *file = NS_STATIC_CAST(nsIFile *, uploadFileProperty);
          nsCString cid;
          cid.AssignLiteral("cid:");
          MakeMultipartContentID(cid);
          rv = attrDOMNode->SetNodeValue(NS_ConvertUTF8toUTF16(cid));
          NS_ENSURE_SUCCESS(rv, rv);
          aAttachments->Append(file, cid);
        }
      }
    }

    nsCOMPtr<nsIDOMNode> child;
    currentNode->GetFirstChild(getter_AddRefs(child));
    if (child) {
      rv = CreateAttachments(aModel, child, aAttachments);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIDOMNode> node;
    currentNode->GetNextSibling(getter_AddRefs(node));
    currentNode.swap(node);
  }
  
  return NS_OK;
}
      
static void
ReleaseObject(void    *aObject,
              nsIAtom *aPropertyName,
              void    *aPropertyValue,
              void    *aData)
{
  NS_STATIC_CAST(nsISupports *, aPropertyValue)->Release();
}

nsresult
nsXFormsSubmissionElement::CopyChildren(nsIModelElementPrivate *aModel,
                                        nsIDOMNode             *aSource,
                                        nsIDOMNode             *aDest,
                                        nsIDOMDocument         *aDestDoc,
                                        const nsString         &aCDATAElements,
                                        PRUint32                aDepth)
{
  nsCOMPtr<nsIDOMNode> currentNode(aSource), node, destChild;

  while (currentNode) {
    // XXX importing the entire node is not quite right here... we also have
    // to iterate over the attributes since the attributes could somehow
    // (remains to be determined) reference external entities.

    aDestDoc->ImportNode(currentNode, PR_FALSE, getter_AddRefs(destChild));
    NS_ENSURE_STATE(destChild);

    PRUint16 type;
    destChild->GetNodeType(&type);
    switch (type) {
      case nsIDOMNode::PROCESSING_INSTRUCTION_NODE: {
        nsCOMPtr<nsIDOMProcessingInstruction> pi = do_QueryInterface(destChild);
        NS_ENSURE_STATE(pi);

        // ignore "<?xml ... ?>" since we would have already inserted this.
        // XXXbeaufour: depends on omit-xml-decl, does it not?

        nsAutoString target;
        pi->GetTarget(target);
        if (!target.EqualsLiteral("xml"))
          aDest->AppendChild(destChild, getter_AddRefs(node));
        break;
      }

      case nsIDOMNode::TEXT_NODE: {
        // honor cdata-section-elements (see xslt spec section 16.1)
        if (aCDATAElements.IsEmpty()) {
          aDest->AppendChild(destChild, getter_AddRefs(node));
        } else {
          currentNode->GetParentNode(getter_AddRefs(node));
          NS_ENSURE_STATE(node);

          nsAutoString name;
          node->GetNodeName(name);
          // check to see if name is mentioned on cdataElements
          if (HasToken(aCDATAElements, name)) {
            nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(destChild);
            NS_ENSURE_STATE(textNode);

            nsAutoString textData;
            textNode->GetData(textData);

            nsCOMPtr<nsIDOMCDATASection> cdataNode;
            aDestDoc->CreateCDATASection(textData, getter_AddRefs(cdataNode));

            aDest->AppendChild(cdataNode, getter_AddRefs(node));
          } else {
            aDest->AppendChild(destChild, getter_AddRefs(node));
          }
        }

        break;
      }

      default: {
        PRUint16 handleNodeResult;
        aModel->HandleInstanceDataNode(currentNode, &handleNodeResult);

        /*
         *  SUBMIT_SERIALIZE_NODE   - node is to be serialized
         *  SUBMIT_SKIP_NODE        - node is not to be serialized
         *  SUBMIT_ABORT_SUBMISSION - abort submission (invalid node or empty required node)
         */
        if (handleNodeResult == nsIModelElementPrivate::SUBMIT_SKIP_NODE) {
          // skip node and subtree
          currentNode->GetNextSibling(getter_AddRefs(node));
          currentNode.swap(node);
          continue;
        } else if (handleNodeResult == nsIModelElementPrivate::SUBMIT_ABORT_SUBMISSION) {
          // abort
          nsXFormsUtils::ReportError(NS_LITERAL_STRING("warnSubmitInvalidNode"),
                                     currentNode, nsIScriptError::warningFlag);
          return NS_ERROR_ILLEGAL_VALUE;
        }

        // If this node has attributes, make sure that we don't copy any
        // that aren't relevant, etc.
        PRBool hasAttrs = PR_FALSE;
        currentNode->HasAttributes(&hasAttrs);
        if ((type == nsIDOMNode::ELEMENT_NODE) && hasAttrs) {
          nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
          nsCOMPtr<nsIDOMNode> attrDOMNode, tempNode;
        
          currentNode->GetAttributes(getter_AddRefs(attrMap));
          NS_ENSURE_STATE(attrMap);
        
          nsresult rv = NS_OK;
          PRUint32 length;
          nsCOMPtr<nsIDOMElement> destElem(do_QueryInterface(destChild));
          attrMap->GetLength(&length);
        
          for (PRUint32 run = 0; run < length; ++run) {
            attrMap->Item(run, getter_AddRefs(attrDOMNode));
            NS_ENSURE_STATE(attrDOMNode);
            aModel->HandleInstanceDataNode(attrDOMNode, &handleNodeResult);

            if (handleNodeResult ==
                       nsIModelElementPrivate::SUBMIT_ABORT_SUBMISSION) {
              // abort
              nsXFormsUtils::ReportError(NS_LITERAL_STRING("warnSubmitInvalidNode"),
                                         currentNode, nsIScriptError::warningFlag);
              return NS_ERROR_ILLEGAL_VALUE;
            }

            nsAutoString localName, namespaceURI;

            rv = attrDOMNode->GetLocalName(localName);
            NS_ENSURE_SUCCESS(rv, rv);
            rv = attrDOMNode->GetNamespaceURI(namespaceURI);
            NS_ENSURE_SUCCESS(rv, rv);

            if (handleNodeResult == nsIModelElementPrivate::SUBMIT_SKIP_NODE) {
              rv = destElem->RemoveAttributeNS(namespaceURI, localName);
              NS_ENSURE_SUCCESS(rv, rv);
            } else {
              // the cloning does not copy any properties of the currentNode. If
              // the attribute node has an uploadFileProperty we need to copy it
              // to the submission document so that local files will be attached
              // properly when the submission format is multipart-related.
              void* uploadFileProperty = nsnull;
              nsCOMPtr<nsIAttribute> attrNode(do_QueryInterface(attrDOMNode));
              if (attrNode) {
                uploadFileProperty =
                  attrNode->GetProperty(nsXFormsAtoms::uploadFileProperty);
                if (uploadFileProperty) {
                  nsCOMPtr<nsIDOMAttr> destDOMAttr;
                  rv = destElem->GetAttributeNodeNS(
                    namespaceURI, localName, getter_AddRefs(destDOMAttr));
                  NS_ENSURE_SUCCESS(rv, rv);
                  nsCOMPtr<nsIAttribute> destAttribute(
                    do_QueryInterface(destDOMAttr));
                  if (destAttribute) {
                    // Clone the local file so the same pointer isn't released
                    // twice
                    nsIFile *file =
                      NS_STATIC_CAST(nsIFile *, uploadFileProperty);
                    nsIFile *fileCopy = nsnull;
                    nsresult rv = file->Clone(&fileCopy);
                    NS_ENSURE_SUCCESS(rv, rv);
                    destAttribute->SetProperty(
                      nsXFormsAtoms::uploadFileProperty, fileCopy,
                      ReleaseObject);
                  }
                }
              }
            }
          }
        }

        // ImportNode does not copy any properties of the currentNode. If the
        // node has an uploadFileProperty we need to copy it to the submission
        // document so that local files will be attached properly when the
        // submission format is multipart-related.
        void* uploadFileProperty = nsnull;
        nsCOMPtr<nsIContent> currentNodeContent(do_QueryInterface(currentNode));
        if (currentNodeContent) {
          uploadFileProperty =
            currentNodeContent->GetProperty(nsXFormsAtoms::uploadFileProperty);
          if (uploadFileProperty) {
            nsCOMPtr<nsIContent> destChildContent(do_QueryInterface(destChild));
            if (destChildContent) {
              // Clone the local file so the same pointer isn't released twice.
              nsIFile *file = NS_STATIC_CAST(nsIFile *, uploadFileProperty);
              nsIFile *fileCopy = nsnull;
              nsresult rv = file->Clone(&fileCopy);
              NS_ENSURE_SUCCESS(rv, rv);
              destChildContent->SetProperty(nsXFormsAtoms::uploadFileProperty,
                                            fileCopy,
                                            ReleaseObject);
            }
          }
        }

        aDest->AppendChild(destChild, getter_AddRefs(node));

        // recurse
        nsCOMPtr<nsIDOMNode> startNode;
        currentNode->GetFirstChild(getter_AddRefs(startNode));

        nsresult rv = CopyChildren(aModel, startNode, destChild, aDestDoc,
                                   aCDATAElements, aDepth + 1);
        NS_ENSURE_SUCCESS(rv, rv);

      }
    }
    
    if (!aDepth) {
      break;
    }

    currentNode->GetNextSibling(getter_AddRefs(node));
    currentNode.swap(node);
  }

  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::SerializeDataURLEncoded(nsIDOMDocument *data,
                                                   nsCString &uri,
                                                   nsIInputStream **stream,
                                                   nsCString &contentType)
{
  // 'get' method:
  // The URI is constructed as follows:
  //  o The submit URI from the action attribute is examined. If it does not
  //    already contain a ? (question mark) character, one is appended. If it
  //    does already contain a question mark character, then a separator
  //    character from the attribute separator is appended.
  //  o The serialized form data is appended to the URI.

  nsCAutoString separator;
  {
    nsAutoString temp;
    mElement->GetAttribute(NS_LITERAL_STRING("separator"), temp);
    if (temp.IsEmpty())
    {
      separator.AssignLiteral(";");
    }
    else
    {
      // Separator per spec can only be |;| or |&|
      if (!temp.EqualsLiteral(";") && !temp.EqualsLiteral("&")) {
        // invalid separator, report the error and abort submission.
        // XXX: we probably should add a visual indicator
        const PRUnichar *strings[] = { temp.get() };
        nsXFormsUtils::ReportError(NS_LITERAL_STRING("invalidSeparator"),
                                   strings, 1, mElement, mElement);
        return NS_ERROR_ILLEGAL_VALUE;
      } else {
        CopyUTF16toUTF8(temp, separator);
      }
    }
  }

  if (mFormat & METHOD_GET)
  {
    if (uri.FindChar('?') == kNotFound)
      uri.Append('?');
    else
      uri.Append(separator);
    AppendURLEncodedData(data, separator, uri);

    *stream = nsnull;
    contentType.Truncate();
  }
  else if (mFormat & METHOD_POST)
  {
    nsCAutoString buf;
    AppendURLEncodedData(data, separator, buf);

    // make new stream
    NS_NewCStringInputStream(stream, buf);
    NS_ENSURE_STATE(*stream);

    contentType.AssignLiteral("application/x-www-form-urlencoded");
  }
  else
  {
    NS_WARNING("unexpected submission format");
    return NS_ERROR_UNEXPECTED;
  }

  // For HTML 4 compatibility sake, trailing separator is to be removed per an
  // upcoming erratum.
  if (StringEndsWith(uri, separator))
    uri.Cut(uri.Length() - 1, 1);

  return NS_OK;
}

void
nsXFormsSubmissionElement::AppendURLEncodedData(nsIDOMNode *data,
                                                const nsCString &separator,
                                                nsCString &buf)
{
  // 1. Each element node is visited in document order. Each element that has
  //    one text node child is selected for inclusion.

  // 2. Element nodes selected for inclusion are encoded as EltName=value{sep},
  //    where = is a literal character, {sep} is the separator character from the
  //    separator attribute on submission, EltName represents the element local
  //    name, and value represents the contents of the text node.

  // NOTE:
  //    The encoding of EltName and value are as follows: space characters are
  //    replaced by +, and then non-ASCII and reserved characters (as defined
  //    by [RFC 2396] as amended by subsequent documents in the IETF track) are
  //    escaped by replacing the character with one or more octets of the UTF-8
  //    representation of the character, with each octet in turn replaced by
  //    %HH, where HH represents the uppercase hexadecimal notation for the
  //    octet value and % is a literal character. Line breaks are represented
  //    as "CR LF" pairs (i.e., %0D%0A).

#ifdef DEBUG_darinf
  nsAutoString nodeName;
  data->GetNodeName(nodeName);
  LOG(("+++ AppendURLEncodedData: inspecting <%s>\n",
      NS_ConvertUTF16toUTF8(nodeName).get()));
#endif

  nsCOMPtr<nsIDOMNode> child;
  data->GetFirstChild(getter_AddRefs(child));
  if (!child)
    return;

  PRUint16 childType;
  child->GetNodeType(&childType);

  nsCOMPtr<nsIDOMNode> sibling;
  child->GetNextSibling(getter_AddRefs(sibling));

  if (!sibling && childType == nsIDOMNode::TEXT_NODE)
  {
    nsAutoString localName;
    data->GetLocalName(localName);

    nsAutoString value;
    child->GetNodeValue(value);

    LOG(("    appending data for <%s>\n", NS_ConvertUTF16toUTF8(localName).get()));

    nsCString encLocalName, encValue;
    URLEncode(localName, encLocalName);
    URLEncode(value, encValue);

    buf.Append(encLocalName + NS_LITERAL_CSTRING("=") + encValue + separator);
  }
  else
  {
    // call AppendURLEncodedData on each child node
    do
    {
      AppendURLEncodedData(child, separator, buf);
      child->GetNextSibling(getter_AddRefs(sibling));
      child.swap(sibling);
    }
    while (child);
  }
}

nsresult
nsXFormsSubmissionElement::SerializeDataMultipartRelated(nsIDOMDocument *data,
                                                         nsIInputStream **stream,
                                                         nsCString &contentType)
{
  NS_ASSERTION(mFormat & METHOD_POST, "unexpected submission method");

  nsCAutoString boundary;
  MakeMultipartBoundary(boundary);

  nsCOMPtr<nsIMultiplexInputStream> multiStream =
      do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  NS_ENSURE_STATE(multiStream);

  nsCAutoString type, start;

  MakeMultipartContentID(start);

  nsresult rv;
  nsCOMPtr<nsIModelElementPrivate> model(GetModel());
  NS_ENSURE_STATE(model);
  SubmissionAttachmentArray attachments;
  rv = CreateAttachments(model, data, &attachments);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> xml;
  rv = SerializeDataXML(data, getter_AddRefs(xml), type);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX we should output a 'charset=' with the 'Content-Type' header

  nsCString postDataChunk;
  postDataChunk += NS_LITERAL_CSTRING("--") + boundary
                +  NS_LITERAL_CSTRING("\r\nContent-Type: ") + type
                +  NS_LITERAL_CSTRING("\r\nContent-ID: <") + start
                +  NS_LITERAL_CSTRING(">\r\n\r\n");
  rv = AppendPostDataChunk(postDataChunk, multiStream);
  NS_ENSURE_SUCCESS(rv, rv);

  multiStream->AppendStream(xml);

  for (PRUint32 i = 0; i < attachments.Count(); ++i) {
    SubmissionAttachment *a = attachments.Item(i);

    nsCOMPtr<nsIInputStream> fileStream;
    nsCAutoString type;
    
    // If the file upload control did not set a file to upload, then
    // we'll upload as if the file selected is empty.
    if (a->file)
    {
      NS_NewLocalFileInputStream(getter_AddRefs(fileStream), a->file);
      NS_ENSURE_SUCCESS(rv, rv);

      GetMimeTypeFromFile(a->file, type);
    }
    else
    {
      type.AssignLiteral("application/octet-stream");
    }

    postDataChunk += NS_LITERAL_CSTRING("\r\n--") + boundary
                  +  NS_LITERAL_CSTRING("\r\nContent-Type: ") + type
                  +  NS_LITERAL_CSTRING("\r\nContent-Transfer-Encoding: binary")
                  +  NS_LITERAL_CSTRING("\r\nContent-ID: <") + a->cid
                  +  NS_LITERAL_CSTRING(">\r\n\r\n");
    rv = AppendPostDataChunk(postDataChunk, multiStream);
    NS_ENSURE_SUCCESS(rv, rv);

    if (fileStream)
      multiStream->AppendStream(fileStream);
  }

  // final boundary
  postDataChunk += NS_LITERAL_CSTRING("\r\n--") + boundary
                +  NS_LITERAL_CSTRING("--\r\n");
  rv = AppendPostDataChunk(postDataChunk, multiStream);
  NS_ENSURE_SUCCESS(rv, rv);

  contentType =
      NS_LITERAL_CSTRING("multipart/related; boundary=") + boundary +
      NS_LITERAL_CSTRING("; type=\"") + type +
      NS_LITERAL_CSTRING("\"; start=\"<") + start +
      NS_LITERAL_CSTRING(">\"");

  NS_ADDREF(*stream = multiStream);
  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::SerializeDataMultipartFormData(nsIDOMDocument *data,
                                                          nsIInputStream **stream,
                                                          nsCString &contentType)
{
  NS_ASSERTION(mFormat & METHOD_POST, "unexpected submission method");

  // This format follows the rules for multipart/form-data MIME data streams in
  // [RFC 2388], with specific requirements of this serialization listed below:
  //  o Each element node is visited in document order.
  //  o Each element that has exactly one text node child is selected for
  //    inclusion.
  //  o Element nodes selected for inclusion are as encoded as
  //    Content-Disposition: form-data MIME parts as defined in [RFC 2387], with
  //    the name parameter being the element local name.
  //  o Element nodes of any datatype populated by upload are serialized as the
  //    specified content and additionally have a Content-Disposition filename
  //    parameter, if available.
  //  o The Content-Type must be text/plain except for xsd:base64Binary,
  //    xsd:hexBinary, and derived types, in which case the header represents the
  //    media type of the attachment if known, otherwise
  //    application/octet-stream. If a character set is applicable, the
  //    Content-Type may have a charset parameter.

  nsCAutoString boundary;
  MakeMultipartBoundary(boundary);

  nsCOMPtr<nsIMultiplexInputStream> multiStream =
      do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  NS_ENSURE_STATE(multiStream);

  nsCString postDataChunk;
  nsresult rv = AppendMultipartFormData(data, boundary, postDataChunk, multiStream);
  NS_ENSURE_SUCCESS(rv, rv);

  postDataChunk += NS_LITERAL_CSTRING("--") + boundary
                +  NS_LITERAL_CSTRING("--\r\n\r\n");
  rv = AppendPostDataChunk(postDataChunk, multiStream);
  NS_ENSURE_SUCCESS(rv, rv);

  contentType = NS_LITERAL_CSTRING("multipart/form-data; boundary=") + boundary;

  NS_ADDREF(*stream = multiStream);
  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::AppendMultipartFormData(nsIDOMNode *data,
                                                   const nsCString &boundary,
                                                   nsCString &postDataChunk,
                                                   nsIMultiplexInputStream *multiStream)
{
#ifdef DEBUG_darinf
  nsAutoString nodeName;
  data->GetNodeName(nodeName);
  LOG(("+++ AppendMultipartFormData: inspecting <%s>\n",
      NS_ConvertUTF16toUTF8(nodeName).get()));
#endif

  nsresult rv;

  nsCOMPtr<nsIDOMNode> child;
  data->GetFirstChild(getter_AddRefs(child));
  if (!child)
    return NS_OK;

  PRUint16 childType;
  child->GetNodeType(&childType);

  nsCOMPtr<nsIDOMNode> sibling;
  child->GetNextSibling(getter_AddRefs(sibling));

  if (!sibling && childType == nsIDOMNode::TEXT_NODE)
  {
    nsAutoString localName;
    data->GetLocalName(localName);

    nsAutoString value;
    child->GetNodeValue(value);

    LOG(("    appending data for <%s>\n", NS_ConvertUTF16toUTF8(localName).get()));

    PRUint32 encType;
    rv = GetElementEncodingType(data, &encType);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ConvertUTF16toUTF8 encName(localName);
    encName.Adopt(nsLinebreakConverter::ConvertLineBreaks(encName.get(),
                  nsLinebreakConverter::eLinebreakAny,
                  nsLinebreakConverter::eLinebreakNet));

    postDataChunk += NS_LITERAL_CSTRING("--") + boundary
                  +  NS_LITERAL_CSTRING("\r\nContent-Disposition: form-data; name=\"")
                  +  encName + NS_LITERAL_CSTRING("\"");

    nsCAutoString contentType;
    nsCOMPtr<nsIInputStream> fileStream;
    if (encType == ELEMENT_ENCTYPE_URI)
    {
      void* uploadFileProperty = nsnull;
      nsCOMPtr<nsIContent> content = do_QueryInterface(data);
      if (content) {
        uploadFileProperty =
          content->GetProperty(nsXFormsAtoms::uploadFileProperty);
      } else {
        nsCOMPtr<nsIAttribute> attr = do_QueryInterface(data);
        NS_ENSURE_STATE(attr);
        uploadFileProperty =
          attr->GetProperty(nsXFormsAtoms::uploadFileProperty);
      }
      
      nsIFile *file = NS_STATIC_CAST(nsIFile *, uploadFileProperty);

      nsAutoString leafName;
      if (file)
      {
        NS_NewLocalFileInputStream(getter_AddRefs(fileStream), file);

        file->GetLeafName(leafName);

        // use mime service to get content-type
        GetMimeTypeFromFile(file, contentType);
      }
      else
      {
        contentType.AssignLiteral("application/octet-stream");
      }

      postDataChunk += NS_LITERAL_CSTRING("; filename=\"")
                    +  NS_ConvertUTF16toUTF8(leafName)
                    +  NS_LITERAL_CSTRING("\"");
    }
    else if (encType == ELEMENT_ENCTYPE_STRING)
    {
      contentType.AssignLiteral("text/plain; charset=UTF-8");
    }
    else
    {
      contentType.AssignLiteral("application/octet-stream");
    }

    postDataChunk += NS_LITERAL_CSTRING("\r\nContent-Type: ")
                  +  contentType
                  +  NS_LITERAL_CSTRING("\r\n\r\n");

    if (encType == ELEMENT_ENCTYPE_URI)
    {
      AppendPostDataChunk(postDataChunk, multiStream);

      if (fileStream)
        multiStream->AppendStream(fileStream);

      postDataChunk += NS_LITERAL_CSTRING("\r\n");
    }
    else
    {
      // for base64Binary and hexBinary types, we assume that the data is
      // already encoded.  this assumption is based on section 8.1.6 of the
      // xforms spec.

      // XXX UTF-8 ok?
      NS_ConvertUTF16toUTF8 encValue(value);
      encValue.Adopt(nsLinebreakConverter::ConvertLineBreaks(encValue.get(),
                     nsLinebreakConverter::eLinebreakAny,
                     nsLinebreakConverter::eLinebreakNet));
      postDataChunk += encValue + NS_LITERAL_CSTRING("\r\n");
    }
  }
  else
  {
    // call AppendMultipartFormData on each child node
    do
    {
      rv = AppendMultipartFormData(child, boundary, postDataChunk, multiStream);
      if (NS_FAILED(rv))
        return rv;
      child->GetNextSibling(getter_AddRefs(sibling));
      child.swap(sibling);
    }
    while (child);
  }
  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::AppendPostDataChunk(nsCString &postDataChunk,
                                               nsIMultiplexInputStream *multiStream)
{
  nsCOMPtr<nsIInputStream> stream;
  NS_NewCStringInputStream(getter_AddRefs(stream), postDataChunk);
  NS_ENSURE_TRUE(stream, NS_ERROR_OUT_OF_MEMORY);

  multiStream->AppendStream(stream);

  postDataChunk.Truncate();
  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::GetElementEncodingType(nsIDOMNode             *node,
                                                  PRUint32               *encType,
                                                  nsIModelElementPrivate *aModel)
{
  *encType = ELEMENT_ENCTYPE_STRING; // default

  // check for 'xsd:base64Binary', 'xsd:hexBinary', or 'xsd:anyURI'
  nsAutoString type, nsuri;
  nsresult rv;
  if (aModel) {
    rv = aModel->GetTypeFromNode(node, type, nsuri);
  } else {
    rv = nsXFormsUtils::ParseTypeFromNode(node, type, nsuri);
  }
  if (NS_SUCCEEDED(rv) &&
      nsuri.EqualsLiteral(NS_NAMESPACE_XML_SCHEMA) &&
      !type.IsEmpty())
  {
    if (type.Equals(NS_LITERAL_STRING("anyURI")))
      *encType = ELEMENT_ENCTYPE_URI;
    else if (type.Equals(NS_LITERAL_STRING("base64Binary")))
      *encType = ELEMENT_ENCTYPE_BASE64;
    else if (type.Equals(NS_LITERAL_STRING("hexBinary")))
      *encType = ELEMENT_ENCTYPE_HEX;

    // XXX need to handle derived types (fixing bug 263384 will help)
  }

  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::CreateFileStream(const nsString &absURI,
                                            nsIFile **resultFile,
                                            nsIInputStream **resultStream)
{
  LOG(("nsXFormsSubmissionElement::CreateFileStream [%s]\n",
      NS_ConvertUTF16toUTF8(absURI).get()));

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), absURI);
  NS_ENSURE_STATE(uri);

  // restrict to file:// -- XXX is this correct?
  PRBool schemeIsFile = PR_FALSE;
  uri->SchemeIs("file", &schemeIsFile);
  NS_ENSURE_STATE(schemeIsFile);

  // NOTE: QI to nsIFileURL just means that the URL corresponds to a 
  // local file resource, which is not restricted to file://
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(uri);
  NS_ENSURE_STATE(fileURL);

  fileURL->GetFile(resultFile);
  NS_ENSURE_STATE(*resultFile);

  return NS_NewLocalFileInputStream(resultStream, *resultFile);
}

nsresult
nsXFormsSubmissionElement::SendData(const nsCString &uriSpec,
                                    nsIInputStream *stream,
                                    const nsCString &contentType)
{
  LOG(("+++ sending to uri=%s [stream=%p]\n", uriSpec.get(), (void*) stream));

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_STATE(doc);

  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  NS_ENSURE_STATE(ios);

  // Any parameters appended to uriSpec are already ASCII-encoded per the rules
  // of section 11.6.  Use our standard document charset based canonicalization
  // for any other non-ASCII bytes.  (This might be important for compatibility
  // with legacy CGI processors.)
  nsCOMPtr<nsIURI> uri;
  ios->NewURI(uriSpec,
              doc->GetDocumentCharacterSet().get(),
              doc->GetDocumentURI(),
              getter_AddRefs(uri));
  NS_ENSURE_STATE(uri);

  nsresult rv;

  // handle mailto: submission
  if (!mIsReplaceInstance) {
    PRBool isMailto;
    rv = uri->SchemeIs("mailto", &isMailto);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isMailto) {
      nsCOMPtr<nsIExternalProtocolService> extProtService =
        do_GetService("@mozilla.org/uriloader/external-protocol-service;1");
      NS_ENSURE_STATE(extProtService);

      PRBool hasExposedMailClient;
      rv = extProtService->ExternalProtocolHandlerExists("mailto",
                                                         &hasExposedMailClient);
      NS_ENSURE_SUCCESS(rv, rv);

      if (hasExposedMailClient) {
        nsCAutoString mailtoUrl(uriSpec);

        // A mailto url looks like this: mailto:foo@bar.com, which can be followed
        // by parameters (subject and body).  The first parameter has to have an
        // "?" before it, and an additional one needs to have an "&".
        // So if "?" already exists in the string, we use "&".

        if (mailtoUrl.Find("&body=") != kNotFound ||
            mailtoUrl.Find("?body=") != kNotFound) {
          // body parameter already exists, so report a warning
          nsXFormsUtils::ReportError(NS_LITERAL_STRING("warnMailtoBodyParam"),
                                     mElement, nsIScriptError::warningFlag);
        }

        if (mailtoUrl.FindChar('?') != kNotFound)
          mailtoUrl.AppendLiteral("&body=");
        else
          mailtoUrl.AppendLiteral("?body=");

        // get the stream contents
        PRUint32 len, read, numReadIn = 1;
        rv = stream->Available(&len);
        NS_ENSURE_SUCCESS(rv, rv);

        char *buf = new char[len+1];
        if (buf == NULL) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        memset(buf, 0, len+1);

        // Read returns 0 if eos
        while (numReadIn != 0) {
          numReadIn = stream->Read(buf, len, &read);
          NS_EscapeURL(buf, read, esc_Query|esc_AlwaysCopy, mailtoUrl);
        }

        delete [] buf;

        // create an nsIUri out of the string
        nsCOMPtr<nsIURI> mailUri;
        ios->NewURI(mailtoUrl,
                    nsnull,
                    nsnull,
                    getter_AddRefs(mailUri));
        NS_ENSURE_STATE(mailUri);

        // let the OS handle the uri
        rv = extProtService->LoadURI(mailUri, nsnull);

        if (NS_FAILED(rv)) {
          // opening an mail client failed.
          nsXFormsUtils::ReportError(NS_LITERAL_STRING("submitMailtoFailed"),
                                     mElement);
          EndSubmit(PR_FALSE);
        } else {
          // the protocol service succeeded
          EndSubmit(PR_TRUE);
        }

      } else {
        // no system mail client found
        nsXFormsUtils::ReportError(NS_LITERAL_STRING("submitMailtoInit"),
                                   mElement);
        EndSubmit(PR_FALSE);
      }

      return NS_OK;
    }
  }

  if (!CheckSameOrigin(doc, uri)) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("submitSendOrigin"),
                               mElement);
    return NS_ERROR_ABORT;
  }

  // wrap the entire upload stream in a buffered input stream, so that
  // it can be read in large chunks.
  // XXX necko should probably do this (or something like this) for us.
  nsCOMPtr<nsIInputStream> bufferedStream;
  if (stream)
  {
    NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), stream, 4096);
    NS_ENSURE_STATE(bufferedStream);
  }

  nsCOMPtr<nsIChannel> channel;
  ios->NewChannelFromURI(uri, getter_AddRefs(channel));
  NS_ENSURE_STATE(channel);

  if (bufferedStream)
  {
    nsCOMPtr<nsIUploadChannel> uploadChannel = do_QueryInterface(channel);
    NS_ENSURE_STATE(uploadChannel);

    // this in effect sets the request method of the channel to 'PUT'
    rv = uploadChannel->SetUploadStream(bufferedStream, contentType, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mFormat & METHOD_POST)
  {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
    NS_ENSURE_STATE(httpChannel);

    rv = httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
    NS_ENSURE_SUCCESS(rv, rv);

    if (mIsSOAPRequest) {
      nsCOMPtr<nsIMIMEHeaderParam> mimeHdrParser =
        do_GetService("@mozilla.org/network/mime-hdrparam;1");
      NS_ENSURE_STATE(mimeHdrParser);

      nsAutoString mediatype, action;
      mElement->GetAttribute(NS_LITERAL_STRING("mediatype"),
                             mediatype);
      if (!mediatype.IsEmpty()) {
        
        rv = mimeHdrParser->GetParameter(NS_ConvertUTF16toUTF8(mediatype),
                                         "action", EmptyCString(), PR_FALSE,
                                         nsnull, action);
      }
      if (action.IsEmpty()) {
        action.AssignLiteral(" ");
      }
      rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("SOAPAction"),
                                         NS_ConvertUTF16toUTF8(action),
                                         PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // set loadGroup and notificationCallbacks

  nsCOMPtr<nsILoadGroup> loadGroup = doc->GetDocumentLoadGroup();
  channel->SetLoadGroup(loadGroup);

  // set LOAD_DOCUMENT_URI so throbber works during submit
  nsLoadFlags loadFlags = 0;
  channel->GetLoadFlags(&loadFlags);
  loadFlags |= nsIChannel::LOAD_DOCUMENT_URI;
  channel->SetLoadFlags(loadFlags);

  // create a pipe in which to store the response (yeah, this kind of
  // sucks since we'll use a lot of memory if the response is large).
  //
  // pipe uses non-blocking i/o since we are just using it for temporary
  // storage.
  //
  // pipe's maximum size is unlimited (gasp!)

  nsCOMPtr<nsIOutputStream> pipeOut;
  rv = NS_NewPipe(getter_AddRefs(mPipeIn), getter_AddRefs(pipeOut),
                  4096, PR_UINT32_MAX, PR_TRUE, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // use a simple stream listener to tee our data into the pipe, and
  // notify us when the channel starts and stops.

  nsCOMPtr<nsIStreamListener> listener;
  rv = NS_NewSimpleStreamListener(getter_AddRefs(listener), pipeOut, this);
  NS_ENSURE_SUCCESS(rv, rv);

  channel->SetNotificationCallbacks(this);
  rv = channel->AsyncOpen(listener, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

// factory constructor

nsresult
NS_NewXFormsSubmissionElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsSubmissionElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
