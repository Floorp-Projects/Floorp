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
#include "nsIXFormsModelElement.h"
#include "nsIXTFGenericElementWrapper.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventGroup.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMXPathExpression.h"
#include "nsIDOMSerializer.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsComponentManagerUtils.h"
#include "nsIWebNavigation.h"
#include "nsIStringStream.h"
#include "nsIInputStream.h"
#include "nsIStorageStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsIMIMEInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIFileURL.h"
#include "nsIMIMEService.h"
#include "nsLinebreakConverter.h"
#include "nsEscape.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"

// namespace literals
#define NAMESPACE_XML_SCHEMA \
        NS_LITERAL_STRING("http://www.w3.org/2001/XMLSchema")
#define NAMESPACE_XML_SCHEMA_INSTANCE \
        NS_LITERAL_STRING("http://www.w3.org/2001/XMLSchema-instance")

static const nsIID sScriptingIIDs[] = {
  NS_IDOMELEMENT_IID,
  NS_IDOMEVENTTARGET_IID,
  NS_IDOM3NODE_IID
};

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

// structure used to store information needed to generate attachments
// for multipart/related submission.
struct SubmissionAttachment
{
  nsString  uri;
  nsCString cid;
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
  nsresult Append(const nsString &uri, const nsCString &cid)
  {
    SubmissionAttachment *a = new SubmissionAttachment;
    if (!a)
      return NS_ERROR_OUT_OF_MEMORY;
    a->uri = uri;
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

NS_IMPL_ISUPPORTS3(nsXFormsSubmissionElement,
                   nsIXTFElement,
                   nsIXTFGenericElement,
                   nsIDOMEventListener)

// nsIXTFElement

NS_IMETHODIMP
nsXFormsSubmissionElement::OnDestroyed()
{
  mElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::GetElementType(PRUint32 *aElementType)
{
  *aElementType = nsIXTFElement::ELEMENT_TYPE_GENERIC_ELEMENT;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::GetIsAttributeHandler(PRBool *aIsAttributeHandler)
{
  *aIsAttributeHandler = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::GetScriptingInterfaces(PRUint32 *aCount, nsIID ***aArray)
{
  return CloneScriptingInterfaces(sScriptingIIDs,
                                  NS_ARRAY_LENGTH(sScriptingIIDs),
                                  aCount, aArray);
}

NS_IMETHODIMP
nsXFormsSubmissionElement::WillChangeDocument(nsIDOMDocument *aNewDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::DocumentChanged(nsIDOMDocument *aNewDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::WillChangeParent(nsIDOMElement *aNewParent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::ParentChanged(nsIDOMElement *aNewParent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::WillInsertChild(nsIDOMNode *aChild, PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::WillAppendChild(nsIDOMNode *aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::ChildAppended(nsIDOMNode *aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::WillRemoveChild(PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::ChildRemoved(PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::WillSetAttribute(nsIAtom *aName,
                                            const nsAString &aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::AttributeSet(nsIAtom *aName, const nsAString &aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::WillRemoveAttribute(nsIAtom *aName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::AttributeRemoved(nsIAtom *aName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::DoneAddingChildren()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSubmissionElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  *aHandled = PR_FALSE;
  return NS_OK;
}

// nsIXTFGenericElement

NS_IMETHODIMP
nsXFormsSubmissionElement::OnCreated(nsIXTFGenericElementWrapper *aWrapper)
{
  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  // We listen on the system event group so that we can check
  // whether preventDefault() was called by any content listeners.

  nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(mElement);
  NS_ASSERTION(receiver, "xml elements must be event receivers");

  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  receiver->GetSystemEventGroup(getter_AddRefs(systemGroup));
  NS_ASSERTION(systemGroup, "system event group must exist");

  nsCOMPtr<nsIDOM3EventTarget> target = do_QueryInterface(mElement);

  target->AddGroupedEventListener(NS_LITERAL_STRING("xforms-submit"),
                                  this, PR_FALSE, systemGroup);
  return NS_OK;
}

// nsIDOMEventListener

NS_IMETHODIMP
nsXFormsSubmissionElement::HandleEvent(nsIDOMEvent *aEvent)
{
  nsCOMPtr<nsIDOMNSUIEvent> evt = do_QueryInterface(aEvent);

  PRBool defaultPrevented;
  evt->GetPreventDefault(&defaultPrevented);
  if (defaultPrevented)
    return NS_OK;

  nsAutoString type;
  aEvent->GetType(type);
  if (type.EqualsLiteral("xforms-submit"))
    Submit();
  return NS_OK;
}

// private methods

void
nsXFormsSubmissionElement::Submit()
{
  LOG(("+++ nsXFormsSubmissionElement::Submit\n"));

  // 1. ensure that we are not currently processing a xforms-submit on our model


  // 2. get selected node from the instance data (use xpath, gives us node
  //    iterator)
  nsCOMPtr<nsIDOMNode> data;
  if (NS_FAILED(GetSelectedInstanceData(getter_AddRefs(data))) || !data)
  {
    NS_WARNING("could not get selected instance data");
    return;
  }


  // 3. revalidate selected instance data (only for namespaces considered for
  //    serialization)

  // XXX call nsISchemaValidator::validate on each node


  // 4. serialize instance data

  PRUint32 format = GetSubmissionFormat(mElement);
  if (format == 0)
  {
    NS_WARNING("unknown submission format");
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  nsCAutoString uri, contentType;
  if (NS_FAILED(SerializeData(data, format, uri, getter_AddRefs(stream),
                              contentType)))
  {
    NS_WARNING("failed to serialize data");
    return;
  }


  // 5. dispatch network request
  
  if (NS_FAILED(SendData(format, uri, stream, contentType)))
  {
    NS_WARNING("failed to send data");
    return;
  }
}

nsresult
nsXFormsSubmissionElement::SubmitEnd(PRBool succeeded)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDOMDocumentEvent> doc = do_QueryInterface(domDoc);

  nsCOMPtr<nsIDOMEvent> event;
  doc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  event->InitEvent(succeeded ? NS_LITERAL_STRING("xforms-submit-done")
                             : NS_LITERAL_STRING("xforms-submit-error"),
                   PR_TRUE, PR_FALSE);

  nsCOMPtr<nsIDOMEventTarget> target;
  if (succeeded)
    target = do_QueryInterface(mElement);
  //else
  //  target = GetModel();

  PRBool cancelled;
  return target->DispatchEvent(event, &cancelled);
}

nsresult
nsXFormsSubmissionElement::GetSelectedInstanceData(nsIDOMNode **result)
{
  // XXX need to support 'instance(id)' xpath function.  for now, we assume
  // that any xpath expression is relative to the first <instance> element.

  nsCOMPtr<nsIDOMNode> instance;
  GetDefaultInstanceData(getter_AddRefs(instance));
  NS_ENSURE_TRUE(instance, NS_ERROR_UNEXPECTED);

  nsAutoString value;
  mElement->GetAttribute(NS_LITERAL_STRING("bind"), value);
  if (value.IsEmpty())
  {
    // inspect 'ref' attribute
    mElement->GetAttribute(NS_LITERAL_STRING("ref"), value);
  }
  else
  {
    // ok, value contains the 'ID' of a <bind> element.
    nsCOMPtr<nsIDOMDocument> doc;
    mElement->GetOwnerDocument(getter_AddRefs(doc));

    nsCOMPtr<nsIDOMElement> bindElement;
    doc->GetElementById(value, getter_AddRefs(bindElement));
    NS_ENSURE_TRUE(bindElement, NS_ERROR_UNEXPECTED);

    bindElement->GetAttribute(NS_LITERAL_STRING("nodeset"), value);
  }

  if (value.IsEmpty())
  {
    // select first <instance> element
    // instance->GetFirstChild(result);
    NS_ADDREF(*result = instance);
    return NS_OK;
  }

  // evaluate 'value' as an xpath expression

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDOMXPathEvaluator> xpath = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(xpath, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMXPathNSResolver> resolver;
  xpath->CreateNSResolver(instance, getter_AddRefs(resolver));
  NS_ENSURE_TRUE(resolver, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISupports> xpathResult;
  xpath->Evaluate(value, instance, resolver,
                  nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE, nsnull,
                  getter_AddRefs(xpathResult));
  nsCOMPtr<nsIDOMXPathResult> nodeset = do_QueryInterface(xpathResult);
  NS_ENSURE_TRUE(nodeset, NS_ERROR_UNEXPECTED);

  return nodeset->GetSingleNodeValue(result);
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
                                         PRUint32 format,
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

  if (format & ENCODING_XML)
    return SerializeDataXML(data, format, stream, contentType, nsnull);

  if (format & ENCODING_URL)
    return SerializeDataURLEncoded(data, format, uri, stream, contentType);

  if (format & ENCODING_MULTIPART_RELATED)
    return SerializeDataMultipartRelated(data, format, stream, contentType);

  if (format & ENCODING_MULTIPART_FORM_DATA)
    return SerializeDataMultipartFormData(data, format, stream, contentType);

  NS_WARNING("unsupported submission encoding");
  return NS_ERROR_UNEXPECTED;
}

nsresult
nsXFormsSubmissionElement::SerializeDataXML(nsIDOMNode *data,
                                            PRUint32 format,
                                            nsIInputStream **stream,
                                            nsCString &contentType,
                                            SubmissionAttachmentArray *attachments)
{
  nsAutoString mediaType;
  mElement->GetAttribute(NS_LITERAL_STRING("mediaType"), mediaType);
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

  nsCOMPtr<nsIDOMDocument> doc;
  data->GetOwnerDocument(getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  // clone and possibly modify the document for submission
  nsCOMPtr<nsIDOMDocument> newDoc;
  CreateSubmissionDoc(doc, encoding, attachments, getter_AddRefs(newDoc));
  NS_ENSURE_TRUE(newDoc, NS_ERROR_UNEXPECTED);
  
  nsresult rv =
      serializer->SerializeToStream(newDoc, sink,
                                    NS_LossyConvertUTF16toASCII(encoding));
  NS_ENSURE_SUCCESS(rv, rv);

  // close the output stream, so that the input stream will not return
  // NS_BASE_STREAM_WOULD_BLOCK when it reaches end-of-stream.
  sink->Close();

  return storage->NewInputStream(0, stream);
}

nsresult
nsXFormsSubmissionElement::CreateSubmissionDoc(nsIDOMDocument *source,
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

  nsCOMPtr<nsIDOMDOMImplementation> impl;
  source->GetImplementation(getter_AddRefs(impl));
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

  CopyChildren(source, doc, doc, attachments, cdataElements, indent, 0);

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
  nsCOMPtr<nsIDOMNode> sourceChild, node, destChild;
  source->GetFirstChild(getter_AddRefs(sourceChild));
  while (sourceChild)
  {
    // if not indenting, then strip all unnecessary whitespace

    // XXX importing the entire node is not quite right here... we also have
    // to iterate over the attributes since the attributes could somehow
    // (remains to be determined) reference external entities.

    destDoc->ImportNode(sourceChild, PR_FALSE, getter_AddRefs(destChild));
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
      // XXX honor cdata-section-elements (see xslt spec section 16.1)
      dest->AppendChild(destChild, getter_AddRefs(node));
    }
    else
    {
      // if |destChild| is an element node of type 'xsd:anyURI', and if we have
      // an attachments array, then we need to perform multipart/related
      // processing (i.e., generate a ContentID for the child of this element,
      // and append a new attachment to the attachments array).

      PRUint32 encType;
      if (attachments &&
          NS_SUCCEEDED(GetElementEncodingType(destChild, &encType)) &&
          encType == ELEMENT_ENCTYPE_URI)
      {
        // ok, looks like we have a URI to process
        sourceChild->GetFirstChild(getter_AddRefs(node));
        NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

        node->GetNodeType(&type);
        NS_ENSURE_TRUE(type == nsIDOMNode::TEXT_NODE, NS_ERROR_UNEXPECTED);

        nsString value;
        node->GetNodeValue(value);

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

        attachments->Append(value, cid);
      }
      else
      {
        dest->AppendChild(destChild, getter_AddRefs(node));

        // recurse
        nsresult rv = CopyChildren(sourceChild, destChild, destDoc, attachments,
                                   cdataElements, indent, depth + 1);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    sourceChild->GetNextSibling(getter_AddRefs(node));
    sourceChild.swap(node);
  }
  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::SerializeDataURLEncoded(nsIDOMNode *data,
                                                   PRUint32 format,
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
      // XXX validate input?  take only the first character?
      CopyUTF16toUTF8(temp, separator);
    }
  }

  if (format & METHOD_GET)
  {
    if (uri.FindChar('?') == kNotFound)
      uri.Append('?');
    else
      uri.Append(separator);
    AppendURLEncodedData(data, separator, uri);

    *stream = nsnull;
    contentType.Truncate();
  }
  else if (format & METHOD_POST)
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
                                                         PRUint32 format,
                                                         nsIInputStream **stream,
                                                         nsCString &contentType)
{
  NS_ASSERTION(format & METHOD_POST, "unexpected submission method");

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
  SerializeDataXML(data, ENCODING_XML | METHOD_POST, getter_AddRefs(xml), type,
                   &attachments);

  // XXX we should output a 'charset=' with the 'Content-Type' header

  nsCString postDataChunk;
  postDataChunk += NS_LITERAL_CSTRING("--") + boundary
                +  NS_LITERAL_CSTRING("\r\nContent-Type: ") + type
                +  NS_LITERAL_CSTRING("\r\nContent-ID: <") + start
                +  NS_LITERAL_CSTRING(">\r\n");
  nsresult rv = AppendPostDataChunk(postDataChunk, multiStream);
  NS_ENSURE_SUCCESS(rv, rv);

  multiStream->AppendStream(xml);

  for (PRUint32 i=0; i<attachments.Count(); ++i)
  {
    SubmissionAttachment *a = attachments.Item(i);

    nsCOMPtr<nsIFile> file;
    nsCOMPtr<nsIInputStream> fileStream;
    rv = CreateFileStream(a->uri,
                          getter_AddRefs(file),
                          getter_AddRefs(fileStream));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString type;
    GetMimeTypeFromFile(file, type);

    postDataChunk += NS_LITERAL_CSTRING("\r\n--") + boundary
                  +  NS_LITERAL_CSTRING("\r\nContent-Type: ") + type
                  +  NS_LITERAL_CSTRING("\r\nContent-Transfer-Encoding: binary")
                  +  NS_LITERAL_CSTRING("\r\nContent-ID: <") + a->cid
                  +  NS_LITERAL_CSTRING(">\r\n");
    rv = AppendPostDataChunk(postDataChunk, multiStream);
    NS_ENSURE_SUCCESS(rv, rv);

    multiStream->AppendStream(fileStream);
  }

  // final boundary
  postDataChunk += NS_LITERAL_CSTRING("\r\n--") + boundary
                +  NS_LITERAL_CSTRING("--\r\n");
  rv = AppendPostDataChunk(postDataChunk, multiStream);
  NS_ENSURE_SUCCESS(rv, rv);

  contentType =
      NS_LITERAL_CSTRING("multipart/related; boundary=") + boundary +
      NS_LITERAL_CSTRING("; type=") + type +
      NS_LITERAL_CSTRING("; start=\"<") + start +
      NS_LITERAL_CSTRING(">\"");

  NS_ADDREF(*stream = multiStream);
  return NS_OK;
}

nsresult
nsXFormsSubmissionElement::SerializeDataMultipartFormData(nsIDOMNode *data,
                                                          PRUint32 format,
                                                          nsIInputStream **stream,
                                                          nsCString &contentType)
{
  NS_ASSERTION(format & METHOD_POST, "unexpected submission method");

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
                +  NS_LITERAL_CSTRING("--\r\n");
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
      // 'value' contains an absolute URI reference
      nsCOMPtr<nsIFile> file;
      rv = CreateFileStream(value, getter_AddRefs(file), getter_AddRefs(fileStream));
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString leafName;
      file->GetLeafName(leafName);

      postDataChunk += NS_LITERAL_CSTRING("; filename=\"")
                    +  NS_ConvertUTF16toUTF8(leafName)
                    +  NS_LITERAL_CSTRING("\"");

      // use mime service to get content-type
      GetMimeTypeFromFile(file, contentType);
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
                  +  contentType + NS_LITERAL_CSTRING("\r\n");

    if (encType == ELEMENT_ENCTYPE_URI)
    {
      AppendPostDataChunk(postDataChunk, multiStream);

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
  element->GetAttributeNS(NAMESPACE_XML_SCHEMA_INSTANCE,
                          NS_LITERAL_STRING("type"), type);
  if (!type.IsEmpty())
  {
    // check for 'xsd:base64binary', 'xsd:hexBinary', or 'xsd:anyURI'

    // XXX need to handle derived types (fixing bug 263384 will help)

    // get 'xsd' namespace prefix
    nsCOMPtr<nsIDOM3Node> dom3Node = do_QueryInterface(node);
    NS_ENSURE_TRUE(dom3Node, NS_ERROR_UNEXPECTED);

    nsAutoString prefix;
    dom3Node->LookupPrefix(NAMESPACE_XML_SCHEMA, prefix);

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
nsXFormsSubmissionElement::SendData(PRUint32 format,
                                    const nsCString &uri,
                                    nsIInputStream *stream,
                                    const nsCString &contentType)
{
  LOG(("+++ sending to uri=%s [stream=%p]\n", uri.get(), (void*) stream));

  // XXX need to properly support the various 'replace' modes and trigger
  //     xforms-submit-done or xforms-submit-error when appropriate.

  nsAutoString replace;
  mElement->GetAttribute(NS_LITERAL_STRING("replace"), replace);
  if (!replace.IsEmpty() && !replace.EqualsLiteral("all"))
  {
    NS_WARNING("replace != 'all' not implemented");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // XXX HACK HACK - wrap with mime stream if we are doing a POST
  if (format & METHOD_POST)
  {
    nsCOMPtr<nsIMIMEInputStream> mimeStream =
        do_CreateInstance("@mozilla.org/network/mime-input-stream;1");
    NS_ENSURE_TRUE(mimeStream, NS_ERROR_UNEXPECTED);

    mimeStream->AddHeader("Content-Type", contentType.get());
    mimeStream->SetAddContentLength(PR_TRUE);
    mimeStream->SetData(stream);

    stream->Release();
    NS_ADDREF(stream = mimeStream);
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

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);

  nsCOMPtr<nsISupports> container = doc->GetContainer();
  nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(container);

  // XXX this is wrong since we need to handle load errors ourselves.

  // XXX we need an API for handing off our channel to the URI loader,
  //     once we decide to load its content into the browser, so that
  //     the URI loader can run its DispatchContent algorithm.
  //     see bug 263084.

  NS_ConvertASCIItoUTF16 temp(uri); // XXX hack
  return webNav->LoadURI(temp.get(), nsIWebNavigation::LOAD_FLAGS_NONE,
                         doc->GetDocumentURI(), bufferedStream, nsnull);
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
