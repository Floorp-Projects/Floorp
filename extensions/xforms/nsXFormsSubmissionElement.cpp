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
#include "nsIXTFGenericElementWrapper.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
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
#include "nsComponentManagerUtils.h"
#include "nsIDocShell.h"
#include "nsIStringStream.h"
#include "nsIInputStream.h"
#include "nsIStorageStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsIMIMEInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
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

// namespace literals
#define NAMESPACE_XML_SCHEMA \
        NS_LITERAL_STRING("http://www.w3.org/2001/XMLSchema")
#define NAMESPACE_XML_SCHEMA_INSTANCE \
        NS_LITERAL_STRING("http://www.w3.org/2001/XMLSchema-instance")
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

// nsIXTFGenericElement

NS_IMETHODIMP
nsXFormsSubmissionElement::OnCreated(nsIXTFGenericElementWrapper *aWrapper)
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

  if (!CheckSameOrigin(doc->GetDocumentURI(), newURI)) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("submitSendOrigin"),
                               mElement);
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::OnStartRequest(nsIRequest *request, nsISupports *ctx)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::OnStopRequest(nsIRequest *request, nsISupports *ctx, nsresult status)
{
  LOG(("xforms submission complete [status=%x]\n", status));

  if (!mElement) {
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  NS_ASSERTION(channel, "request should be a channel");

  PRBool succeeded = NS_SUCCEEDED(status);
  if (succeeded)
  {
    PRUint32 avail = 0;
    mPipeIn->Available(&avail);
    if (avail > 0)
    {

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
  nsresult rv;
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
    instanceElement->SetDocument(newDoc);

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

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
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

  nsresult rv;

  // 1. ensure that we are not currently processing a xforms-submit (see E37)
  NS_ENSURE_STATE(!mSubmissionActive);
  mSubmissionActive = PR_TRUE;
  
  if (mActivator)
    mActivator->SetDisabled(PR_TRUE);

  // Someone may change the "replace" attribute during submission
  // and that would break the ::SameOriginCheck().
  nsAutoString replace;
  mElement->GetAttribute(NS_LITERAL_STRING("replace"), replace);
  mIsReplaceInstance = replace.EqualsLiteral("instance");
  
  // XXX seems to be required by 
  // http://www.w3.org/TR/2003/REC-xforms-20031014/slice4.html#evt-revalidate
  // but is it really needed for us?
  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  model->Recalculate();
  model->Revalidate();

  // 2. get selected node from the instance data (use xpath, gives us node
  //    iterator)
  nsCOMPtr<nsIDOMNode> data;
  rv = GetBoundInstanceData(getter_AddRefs(data));
  NS_ENSURE_SUCCESS(rv, rv);

  // No data to submit
  if (!data) {
    EndSubmit(PR_FALSE);
    return NS_OK;
  }

  // 3. revalidate selected instance data (only for namespaces considered for
  //    serialization)

  // XXX call nsISchemaValidator::validate on each node


  // 4. serialize instance data
  // Checking the format only before starting the submission.
  mFormat = GetSubmissionFormat(mElement);
  NS_ENSURE_STATE(mFormat != 0);

  nsCOMPtr<nsIInputStream> stream;
  nsCAutoString uri, contentType;
  rv = SerializeData(data, uri, getter_AddRefs(stream), contentType);
  NS_ENSURE_SUCCESS(rv, rv);


  // 5. dispatch network request
  
  rv = SendData(uri, stream, contentType);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsXFormsSubmissionElement::GetBoundInstanceData(nsIDOMNode **result)
{
  nsCOMPtr<nsIModelElementPrivate> model;
  nsCOMPtr<nsIDOMXPathResult> xpRes;
  nsresult rv =
    nsXFormsUtils::EvaluateNodeBinding(mElement, 0,
                                       NS_LITERAL_STRING("ref"),
                                       NS_LITERAL_STRING("/"),
                                       nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                       getter_AddRefs(model),
                                       getter_AddRefs(xpRes));

  if (NS_FAILED(rv) || !xpRes)
    return NS_ERROR_UNEXPECTED;

  return xpRes->GetSingleNodeValue(result);
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
nsXFormsSubmissionElement::SerializeData(nsIDOMNode *data,
                                         nsCString &uri,
                                         nsIInputStream **stream,
                                         nsCString &contentType)
{
  // initialize uri to the given action
  nsAutoString action;
  mElement->GetAttribute(NS_LITERAL_STRING("action"), action);
  CopyUTF16toUTF8(action, uri);

  // 'get' method:
  // The URI is constructed as follows:
  //  o The submit URI from the action attribute is examined. If it does not
  //    already contain a ? (question mark) character, one is appended. If it
  //    does already contain a question mark character, then a separator
  //    character from the attribute separator is appended.
  //  o The serialized form data is appended to the URI.

  if (mFormat & ENCODING_XML)
    return SerializeDataXML(data, stream, contentType, nsnull);

  if (mFormat & ENCODING_URL)
    return SerializeDataURLEncoded(data, uri, stream, contentType);

  if (mFormat & ENCODING_MULTIPART_RELATED)
    return SerializeDataMultipartRelated(data, stream, contentType);

  if (mFormat & ENCODING_MULTIPART_FORM_DATA)
    return SerializeDataMultipartFormData(data, stream, contentType);

  NS_WARNING("unsupported submission encoding");
  return NS_ERROR_UNEXPECTED;
}

nsresult
nsXFormsSubmissionElement::SerializeDataXML(nsIDOMNode *data,
                                            nsIInputStream **stream,
                                            nsCString &contentType,
                                            SubmissionAttachmentArray *attachments)
{
  nsAutoString mediaType;
  mElement->GetAttribute(NS_LITERAL_STRING("mediatype"), mediaType);
  if (mediaType.IsEmpty())
    contentType.AssignLiteral("application/xml");
  else
    CopyUTF16toUTF8(mediaType, contentType);

  nsAutoString encoding;
  mElement->GetAttribute(NS_LITERAL_STRING("encoding"), encoding);
  if (encoding.IsEmpty())
    encoding.AssignLiteral("UTF-8");

  nsCOMPtr<nsIStorageStream> storage;
  NS_NewStorageStream(4096, PR_UINT32_MAX, getter_AddRefs(storage));
  NS_ENSURE_TRUE(storage, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIOutputStream> sink;
  storage->GetOutputStream(0, getter_AddRefs(sink));
  NS_ENSURE_TRUE(sink, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIDOMSerializer> serializer =
      do_GetService("@mozilla.org/xmlextras/xmlserializer;1");
  NS_ENSURE_TRUE(serializer, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMDocument> doc, newDoc;
  data->GetOwnerDocument(getter_AddRefs(doc));

  nsresult rv;
  // XXX: We can't simply pass in data if !doc, since it crashes
  if (!doc) {
    // owner doc is null when the data node is the document (e.g., ref="/")
    // so we can just get the document via QI.
    doc = do_QueryInterface(data);
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    rv = CreateSubmissionDoc(doc, encoding, attachments,
                             getter_AddRefs(newDoc));
  } else {
    // if we got a document, we need to create a new
    rv = CreateSubmissionDoc(data, encoding, attachments,
                             getter_AddRefs(newDoc));
  }

  // clone and possibly modify the document for submission
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(newDoc, NS_ERROR_UNEXPECTED);

  // We now need to add namespaces to the submission document.  We get them
  // from 3 sources - the main document's documentElement, the model and the
  // xforms:instance that contains the submitted instance data node.

  // first handle the main document
  nsCOMPtr<nsIDOMDocument> mainDoc;
  mElement->GetOwnerDocument(getter_AddRefs(mainDoc));
  NS_ENSURE_TRUE(mainDoc, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMElement> docElm;
  mainDoc->GetDocumentElement(getter_AddRefs(docElm));
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(docElm));
  NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

  // get the document element of the document we are going to submit
  nsCOMPtr<nsIDOMElement> newDocElm;
  newDoc->GetDocumentElement(getter_AddRefs(newDocElm));

  nsCOMPtr<nsIDOMNode> instanceNode;
  rv = nsXFormsUtils::GetInstanceNodeForData(data, getter_AddRefs(instanceNode));
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
    nsStringHashSet* prefixHash = nsnull;
    PRBool hasPrefixAttr = PR_FALSE;
    mElement->HasAttribute(kIncludeNamespacePrefixes, &hasPrefixAttr);
    if (hasPrefixAttr) {
      rv = GetIncludeNSPrefixesAttr(&prefixHash);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // handle namespace on main document
    rv = AddNameSpaces(newDocElm, node, prefixHash);
    NS_ENSURE_SUCCESS(rv, rv);

    // handle namespaces on the model
    nsCOMPtr<nsIModelElementPrivate> model = GetModel();
    node = do_QueryInterface(model);
    NS_ENSURE_STATE(node);
    rv = AddNameSpaces(newDocElm, node, prefixHash);
    NS_ENSURE_SUCCESS(rv, rv);

    // handle namespaces on the xforms:instance
    rv = AddNameSpaces(newDocElm, instanceNode, prefixHash);
    NS_ENSURE_SUCCESS(rv, rv);

    // handle namespaces on the root element of the instance document
    doc->GetDocumentElement(getter_AddRefs(docElm));
    rv = AddNameSpaces(newDocElm, docElm, prefixHash);
    NS_ENSURE_SUCCESS(rv, rv);

    if (prefixHash)
      delete prefixHash;
  }

  // Serialize content
  rv = serializer->SerializeToStream(newDoc, sink,
                                     NS_LossyConvertUTF16toASCII(encoding));
  NS_ENSURE_SUCCESS(rv, rv);

  // close the output stream, so that the input stream will not return
  // NS_BASE_STREAM_WOULD_BLOCK when it reaches end-of-stream.
  sink->Close();

  return storage->NewInputStream(0, stream);
}

PRBool
nsXFormsSubmissionElement::CheckSameOrigin(nsIURI *aBaseURI, nsIURI *aTestURI)
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

    // if we don't replace the instance, we allow file:// to submit data anywhere
    if (!mIsReplaceInstance) {
      aBaseURI->SchemeIs("file", &allowSubmission);
    }

    // let's check the permission manager
    if (!allowSubmission) {
      allowSubmission = CheckPermissionManager(aBaseURI);
    }

    // if none of the above checks have allowed the submission, we do a
    // same origin check.
    if (!allowSubmission) {
      allowSubmission = nsXFormsUtils::CheckSameOrigin(aBaseURI, aTestURI);
    }
  }

  return allowSubmission;
}

PRBool
nsXFormsSubmissionElement::CheckPermissionManager(nsIURI *aBaseURI)
{
  PRBool result = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);

  PRUint32 permission = nsIPermissionManager::UNKNOWN_ACTION;

  if (NS_SUCCEEDED(rv) && prefBranch) {
    // check if the user has enabled the xforms cross domain preference
    PRBool checkPermission = PR_FALSE;
    prefBranch->GetBoolPref("xforms.crossdomain.enabled", &checkPermission);

    if (checkPermission) {
      // if the user enabled the cross domain check, query the permission
      // manager with the URI.  It will return 1 if the URI was allowed by the
      // user.
      nsCOMPtr<nsIPermissionManager> permissionManager =
        do_GetService("@mozilla.org/permissionmanager;1");

      nsCOMPtr<nsIDOMDocument> domDoc;
      mElement->GetOwnerDocument(getter_AddRefs(domDoc));

      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
      NS_ENSURE_STATE(doc);

      permissionManager->TestPermission(doc->GetDocumentURI(),
                                       "xforms-xd", &permission);
    }
  }

  if (permission == nsIPermissionManager::ALLOW_ACTION) {
    // not in the permission manager
    result = PR_TRUE;
  }

  return result;
}

nsresult
nsXFormsSubmissionElement::AddNameSpaces(nsIDOMElement* aTarget,
                                         nsIDOMNode* aSource,
                                         nsStringHashSet* aPrefixHash)
{
  nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
  nsCOMPtr<nsIDOMNode> attrNode;
  nsAutoString nsURI, localName, value, attrName;

  aSource->GetAttributes(getter_AddRefs(attrMap));
  NS_ENSURE_TRUE(attrMap, NS_ERROR_UNEXPECTED);

  PRUint32 length;
  attrMap->GetLength(&length);

  for (PRUint32 run = 0; run < length; ++run) {
    attrMap->Item(run, getter_AddRefs(attrNode));
    attrNode->GetNamespaceURI(nsURI);

    if (nsURI.Equals(kXMLNSNameSpaceURI)) {
      attrNode->GetLocalName(localName);
      attrNode->GetNodeValue(value);

      attrName.AssignLiteral("xmlns");
      // xmlns:foo, not xmlns=
      if (!localName.EqualsLiteral("xmlns")) {
        // If "includenamespaceprefixes" attribute is present, don't add
        // namespace unless it appears in hash
        if (aPrefixHash && !aPrefixHash->Contains(localName))
          continue;
        attrName.AppendLiteral(":");
        attrName.Append(localName);
      }

      aTarget->SetAttribute(attrName, value);
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
nsXFormsSubmissionElement::CreateSubmissionDoc(nsIDOMNode *source,
                                               const nsString &encoding,
                                               SubmissionAttachmentArray *attachments,
                                               nsIDOMDocument **result)
{
  PRBool indent = GetBooleanAttr(NS_LITERAL_STRING("indent"), PR_FALSE);
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
  NS_ENSURE_TRUE(impl, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMDocument> doc;
  impl->CreateDocument(EmptyString(), EmptyString(), nsnull,
                       getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  if (!omit_xml_declaration)
  {
    nsAutoString buf
        = NS_LITERAL_STRING("version=\"1.0\" encoding=\"") + encoding
        + NS_LITERAL_STRING("\"");

    nsCOMPtr<nsIDOMProcessingInstruction> pi;
    doc->CreateProcessingInstruction(NS_LITERAL_STRING("xml"), buf,
                                     getter_AddRefs(pi));

    nsCOMPtr<nsIDOMNode> newChild;
    doc->AppendChild(pi, getter_AddRefs(newChild));
  }

  // recursively walk the source document, copying nodes as appropriate
  nsCOMPtr<nsIDOMNode> startNode;
  // if it is a document, get the root element
  if (sourceDoc) {
    nsCOMPtr<nsIDOMElement> elm;
    sourceDoc->GetDocumentElement(getter_AddRefs(elm));
    startNode = elm;
  } else {
    startNode = source;
  }

  nsresult rv = CopyChildren(startNode, doc, doc, attachments, cdataElements,
                             indent, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*result = doc);
  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::CopyChildren(nsIDOMNode *source, nsIDOMNode *dest,
                                        nsIDOMDocument *destDoc,
                                        SubmissionAttachmentArray *attachments,
                                        const nsString &cdataElements,
                                        PRBool indent, PRUint32 depth)
{
  nsCOMPtr<nsIDOMNode> currentNode(source), node, destChild;
  nsCOMPtr<nsIModelElementPrivate> model = GetModel();

  while (currentNode)
  {
    // if not indenting, then strip all unnecessary whitespace

    // XXX importing the entire node is not quite right here... we also have
    // to iterate over the attributes since the attributes could somehow
    // (remains to be determined) reference external entities.

    destDoc->ImportNode(currentNode, PR_FALSE, getter_AddRefs(destChild));
    NS_ENSURE_TRUE(destChild, NS_ERROR_UNEXPECTED);

    PRUint16 type;
    destChild->GetNodeType(&type);
    if (type == nsIDOMNode::PROCESSING_INSTRUCTION_NODE)
    {
      nsCOMPtr<nsIDOMProcessingInstruction> pi = do_QueryInterface(destChild);
      NS_ENSURE_TRUE(pi, NS_ERROR_UNEXPECTED);

      // ignore "<?xml ... ?>" since we would have already inserted this.

      nsAutoString target;
      pi->GetTarget(target);
      if (!target.EqualsLiteral("xml"))
        dest->AppendChild(destChild, getter_AddRefs(node));
    }
    else if (type == nsIDOMNode::TEXT_NODE)
    {
      // honor cdata-section-elements (see xslt spec section 16.1)
      if (cdataElements.IsEmpty())
      {
        dest->AppendChild(destChild, getter_AddRefs(node));
      }
      else
      {
        currentNode->GetParentNode(getter_AddRefs(node));
        NS_ENSURE_STATE(node);

        nsAutoString name;
        node->GetNodeName(name);
        // check to see if name is mentioned on cdataElements
        if (HasToken(cdataElements, name))
        {
          nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(destChild);
          NS_ENSURE_STATE(textNode);

          nsAutoString textData;
          textNode->GetData(textData);

          nsCOMPtr<nsIDOMCDATASection> cdataNode;
          destDoc->CreateCDATASection(textData, getter_AddRefs(cdataNode));

          dest->AppendChild(cdataNode, getter_AddRefs(node));
        }
        else
        {
          dest->AppendChild(destChild, getter_AddRefs(node));
        }
      }
    }
    else
    {

     PRUint16 handleNodeResult;
     model->HandleInstanceDataNode(currentNode, &handleNodeResult);

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
        return NS_ERROR_ILLEGAL_VALUE;
      }

      // if |destChild| is an element node of type 'xsd:anyURI', and if we have
      // an attachments array, then we need to perform multipart/related
      // processing (i.e., generate a ContentID for the child of this element,
      // and append a new attachment to the attachments array).

      PRUint32 encType;
      if (attachments &&
          NS_SUCCEEDED(GetElementEncodingType(destChild, &encType)) &&
          encType == ELEMENT_ENCTYPE_URI)
      {
        // ok, looks like we have a local file to upload

        nsCOMPtr<nsIContent> content = do_QueryInterface(currentNode);
        NS_ENSURE_STATE(content);

        nsILocalFile *file =
            NS_STATIC_CAST(nsILocalFile *,
                           content->GetProperty(nsXFormsAtoms::uploadFileProperty));
        // NOTE: this value may be null if a file hasn't been selected.

        nsCString cid;
        MakeMultipartContentID(cid);

        nsAutoString cidURI;
        cidURI.AssignLiteral("cid:");
        AppendASCIItoUTF16(cid, cidURI);

        nsCOMPtr<nsIDOMText> text;
        destDoc->CreateTextNode(cidURI, getter_AddRefs(text));
        NS_ENSURE_TRUE(text, NS_ERROR_UNEXPECTED);

        destChild->AppendChild(text, getter_AddRefs(node));
        dest->AppendChild(destChild, getter_AddRefs(node));

        attachments->Append(file, cid);
      }
      else
      {
        dest->AppendChild(destChild, getter_AddRefs(node));

        // recurse
        nsCOMPtr<nsIDOMNode> startNode;
        currentNode->GetFirstChild(getter_AddRefs(startNode));

        nsresult rv = CopyChildren(startNode, destChild, destDoc, attachments,
                                   cdataElements, indent, depth + 1);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    currentNode->GetNextSibling(getter_AddRefs(node));
    currentNode.swap(node);
  }
  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::SerializeDataURLEncoded(nsIDOMNode *data,
                                                   nsCString &uri,
                                                   nsIInputStream **stream,
                                                   nsCString &contentType)
{
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
    NS_ENSURE_TRUE(*stream, NS_ERROR_UNEXPECTED);

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
nsXFormsSubmissionElement::SerializeDataMultipartRelated(nsIDOMNode *data,
                                                         nsIInputStream **stream,
                                                         nsCString &contentType)
{
  NS_ASSERTION(mFormat & METHOD_POST, "unexpected submission method");

  nsCAutoString boundary;
  MakeMultipartBoundary(boundary);

  nsCOMPtr<nsIMultiplexInputStream> multiStream =
      do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  NS_ENSURE_TRUE(multiStream, NS_ERROR_UNEXPECTED);

  nsCAutoString type, start;

  MakeMultipartContentID(start);

  // XXX we need to extend SerializeDataXML so that it has a mode in which it
  //     generates ContentIDs for elements of type xsd:anyURI, and returns a
  //     list of ContentID <-> URI mappings.

  nsCOMPtr<nsIInputStream> xml;
  SubmissionAttachmentArray attachments;
  SerializeDataXML(data, getter_AddRefs(xml), type, &attachments);

  // XXX we should output a 'charset=' with the 'Content-Type' header

  nsCString postDataChunk;
  postDataChunk += NS_LITERAL_CSTRING("--") + boundary
                +  NS_LITERAL_CSTRING("\r\nContent-Type: ") + type
                +  NS_LITERAL_CSTRING("\r\nContent-ID: <") + start
                +  NS_LITERAL_CSTRING(">\r\n\r\n");
  nsresult rv = AppendPostDataChunk(postDataChunk, multiStream);
  NS_ENSURE_SUCCESS(rv, rv);

  multiStream->AppendStream(xml);

  for (PRUint32 i=0; i<attachments.Count(); ++i)
  {
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
nsXFormsSubmissionElement::SerializeDataMultipartFormData(nsIDOMNode *data,
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
  NS_ENSURE_TRUE(multiStream, NS_ERROR_UNEXPECTED);

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
      nsCOMPtr<nsIContent> content = do_QueryInterface(data);
      NS_ENSURE_STATE(content);

      nsILocalFile *file =
          NS_STATIC_CAST(nsILocalFile *,
                         content->GetProperty(nsXFormsAtoms::uploadFileProperty));

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
      // for base64binary and hexBinary types, we assume that the data is
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
nsXFormsSubmissionElement::GetElementEncodingType(nsIDOMNode *node, PRUint32 *encType)
{
  *encType = ELEMENT_ENCTYPE_STRING; // default

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
  NS_ENSURE_TRUE(element, NS_ERROR_UNEXPECTED);

  nsAutoString type;
  element->GetAttributeNS(NS_LITERAL_STRING(NS_NAMESPACE_XML_SCHEMA_INSTANCE),
                          NS_LITERAL_STRING("type"), type);
  if (!type.IsEmpty())
  {
    // check for 'xsd:base64binary', 'xsd:hexBinary', or 'xsd:anyURI'

    // XXX need to handle derived types (fixing bug 263384 will help)

    // get 'xsd' namespace prefix
    nsCOMPtr<nsIDOM3Node> dom3Node = do_QueryInterface(node);
    NS_ENSURE_TRUE(dom3Node, NS_ERROR_UNEXPECTED);

    nsAutoString prefix;
    dom3Node->LookupPrefix(NS_LITERAL_STRING(NS_NAMESPACE_XML_SCHEMA), prefix);

    if (prefix.IsEmpty())
    {
      NS_WARNING("namespace prefix not found! -- assuming 'xsd'");
      prefix.AssignLiteral("xsd"); // XXX HACK HACK HACK
    }

    if (type.Length() > prefix.Length() &&
        prefix.Equals(StringHead(type, prefix.Length())) &&
        type.CharAt(prefix.Length()) == PRUnichar(':'))
    {
      const nsSubstring &tail = Substring(type, prefix.Length() + 1);
      if (tail.Equals(NS_LITERAL_STRING("anyURI")))
        *encType = ELEMENT_ENCTYPE_URI;
      else if (tail.Equals(NS_LITERAL_STRING("base64binary")))
        *encType = ELEMENT_ENCTYPE_BASE64;
      else if (tail.Equals(NS_LITERAL_STRING("hexBinary")))
        *encType = ELEMENT_ENCTYPE_HEX;
    }
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
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  // restrict to file:// -- XXX is this correct?
  PRBool schemeIsFile = PR_FALSE;
  uri->SchemeIs("file", &schemeIsFile);
  NS_ENSURE_TRUE(schemeIsFile, NS_ERROR_UNEXPECTED);

  // NOTE: QI to nsIFileURL just means that the URL corresponds to a 
  // local file resource, which is not restricted to file://
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(uri);
  NS_ENSURE_TRUE(fileURL, NS_ERROR_UNEXPECTED);

  fileURL->GetFile(resultFile);
  NS_ENSURE_TRUE(*resultFile, NS_ERROR_UNEXPECTED);

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

  if (!CheckSameOrigin(doc->GetDocumentURI(), uri)) {
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
    NS_ENSURE_TRUE(bufferedStream, NS_ERROR_UNEXPECTED);
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
  }

  // set loadGroup and notificationCallbacks

  nsCOMPtr<nsILoadGroup> loadGroup = doc->GetDocumentLoadGroup();
  channel->SetLoadGroup(loadGroup);

  // create a pipe in which to store the response (yeah, this kind of
  // sucks since we'll use a lot of memory if the response is large).
  //
  // pipe uses non-blocking i/o since we are just using it for temporary
  // storage.
  //
  // pipe's maximum size is unlimited (gasp!)

  nsCOMPtr<nsIOutputStream> pipeOut;
  rv = NS_NewPipe(getter_AddRefs(mPipeIn), getter_AddRefs(pipeOut),
                  0, PR_UINT32_MAX, PR_TRUE, PR_TRUE);
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
