/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsMetricsService.h"
#include "nsStringUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMParser.h"
#include "nsIDOMElement.h"
#include "nsIDOM3Node.h"
#include "nsIFileStreams.h"
#include "nsILocalFile.h"
#include "nsComponentManagerUtils.h"
#include "nsNetCID.h"
#include "prprf.h"
#include "nsTArray.h"
#include "nsIDOMSerializer.h"

#define NS_DEFAULT_UPLOAD_INTERVAL_SEC 60 * 5

//-----------------------------------------------------------------------------

static const nsString
MakeKey(const nsAString &eventNS, const nsAString &eventName)
{
  // Since eventName must be a valid XML NCName, we can use ':' to separate
  // eventName from eventNS when formulating our hash key.
  NS_ASSERTION(FindChar(eventName, ':') == -1, "Not a valid NCName");

  nsString key;
  key.Append(eventName);
  key.Append(':');
  key.Append(eventNS);
  return key;
}

static void
SplitKey(const nsString &key, nsString &eventNS, nsString &eventName)
{
  PRInt32 colon = FindChar(key, ':');
  if (colon == -1) {
    NS_ERROR("keys from MakeKey should always have a colon");
    return;
  }

  eventName = Substring(key, 0, colon);
  eventNS = Substring(key, colon + 1, key.Length() - colon - 1);
}

// This method leaves the result value unchanged if a parsing error occurs.
static void
ReadIntegerAttr(nsIDOMElement *elem, const nsAString &attrName, PRInt32 *result)
{
  nsString attrValue;
  elem->GetAttribute(attrName, attrValue);

  NS_ConvertUTF16toUTF8 attrValueUtf8(attrValue);
  PR_sscanf(attrValueUtf8.get(), "%ld", result);
}

//-----------------------------------------------------------------------------

nsMetricsConfig::nsMetricsConfig()
{
}

PRBool
nsMetricsConfig::Init()
{
  if (!mEventSet.Init() || !mNSURIToPrefixMap.Init()) {
    return PR_FALSE;
  }
  Reset();
  return PR_TRUE;
}

void
nsMetricsConfig::Reset()
{
  // By default, we have no event limit, but all collectors are disabled
  // until we're told by the server to enable them.
  NS_ASSERTION(mEventSet.IsInitialized(), "nsMetricsConfig::Init not called");

  mEventSet.Clear();
  mNSURIToPrefixMap.Clear();
  mEventLimit = PR_INT32_MAX;
  mUploadInterval = NS_DEFAULT_UPLOAD_INTERVAL_SEC;
  mHasConfig = PR_FALSE;
}

nsresult
nsMetricsConfig::Load(nsIFile *file)
{
  // The given file references a XML file with the following structure:
  //
  // <response xmlns="http://www.mozilla.org/metrics">
  //   <config xmlns:foo="http://foo.com/metrics">
  //     <collectors>
  //       <collector type="ui"/>
  //       <collector type="document"/>
  //       <collector type="window"/>
  //       <collector type="foo:mystat"/>
  //     </collectors>
  //     <limit events="200"/>
  //     <upload interval="600"/>
  //   </config>
  // </response>

  NS_ASSERTION(mEventSet.IsInitialized(), "nsMetricsConfig::Init not called");

  PRInt64 fileSize;
  nsresult rv = file->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(fileSize <= PR_INT32_MAX);

  nsCOMPtr<nsIFileInputStream> stream =
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID);
  NS_ENSURE_STATE(stream);
  rv = stream->Init(file, -1, -1, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMParser> parser = do_CreateInstance(NS_DOMPARSER_CONTRACTID);
  NS_ENSURE_STATE(parser);

  nsCOMPtr<nsIDOMDocument> doc;
  parser->ParseFromStream(stream, nsnull, PRInt32(fileSize), "application/xml",
                          getter_AddRefs(doc));
  NS_ENSURE_STATE(doc);

  // Now, walk the DOM.  Most elements are optional, but we check the root
  // element to make sure it's a valid response document.
  nsCOMPtr<nsIDOMElement> elem;
  doc->GetDocumentElement(getter_AddRefs(elem));
  NS_ENSURE_STATE(elem);

  nsString nameSpace;
  elem->GetNamespaceURI(nameSpace);
  if (!nameSpace.Equals(NS_LITERAL_STRING(NS_METRICS_NAMESPACE))) {
    // We have a root element, but it's the wrong namespace
    return NS_ERROR_FAILURE;
  }

  nsString tag;
  elem->GetLocalName(tag);
  if (!tag.Equals(NS_LITERAL_STRING("response"))) {
    // The root tag isn't what we're expecting
    return NS_ERROR_FAILURE;
  }

  // At this point, we can clear our old configuration.
  Reset();

  ForEachChildElement(elem, &nsMetricsConfig::ProcessToplevelElement);
  return NS_OK;
}

nsresult
nsMetricsConfig::Save(nsILocalFile *file)
{
  nsCOMPtr<nsIDOMDocument> doc =
    do_CreateInstance("@mozilla.org/xml/xml-document;1");
  NS_ENSURE_STATE(doc);

  nsCOMPtr<nsIDOMElement> response;
  nsMetricsUtils::CreateElement(doc, NS_LITERAL_STRING("response"),
                                getter_AddRefs(response));
  NS_ENSURE_STATE(response);

  nsCOMPtr<nsIDOMElement> config;
  nsMetricsUtils::CreateElement(doc, NS_LITERAL_STRING("config"),
                                getter_AddRefs(config));
  NS_ENSURE_STATE(config);

  nsCOMPtr<nsIDOMElement> collectors;
  nsMetricsUtils::CreateElement(doc, NS_LITERAL_STRING("collectors"),
                                getter_AddRefs(collectors));
  NS_ENSURE_STATE(collectors);

  nsTArray<nsString> events;
  GetEvents(events);

  nsCOMPtr<nsIDOMNode> nodeOut;
  nsresult rv;

  for (PRUint32 i = 0; i < events.Length(); ++i) {
    nsString eventNS, eventName;
    SplitKey(events[i], eventNS, eventName);

    nsString prefix;
    if (!eventNS.Equals(NS_LITERAL_STRING(NS_METRICS_NAMESPACE))) {
      if (!mNSURIToPrefixMap.Get(eventNS, &prefix)) {
        MS_LOG(("uri %s not in prefix map",
                NS_ConvertUTF16toUTF8(eventNS).get()));
        continue;
      }

      // Declare the namespace prefix on the root element
      nsString attrName(NS_LITERAL_STRING("xmlns:"));
      attrName.Append(prefix);
      response->SetAttribute(attrName, eventNS);
    }

    nsCOMPtr<nsIDOMElement> collector;
    nsMetricsUtils::CreateElement(doc, NS_LITERAL_STRING("collector"),
                                  getter_AddRefs(collector));
    NS_ENSURE_STATE(collector);

    nsString shortName;
    if (!prefix.IsEmpty()) {
      shortName = prefix;
      shortName.Append(':');
    }
    shortName.Append(eventName);

    collector->SetAttribute(NS_LITERAL_STRING("type"), eventName);
    collectors->AppendChild(collector, getter_AddRefs(nodeOut));
  }
  config->AppendChild(collectors, getter_AddRefs(nodeOut));

  if (mEventLimit != PR_INT32_MAX) {
    nsCOMPtr<nsIDOMElement> limit;
    nsMetricsUtils::CreateElement(doc, NS_LITERAL_STRING("limit"),
                                  getter_AddRefs(limit));
    NS_ENSURE_STATE(limit);

    nsString limitStr;
    AppendInt(limitStr, mEventLimit);
    limit->SetAttribute(NS_LITERAL_STRING("events"), limitStr);
    config->AppendChild(limit, getter_AddRefs(nodeOut));
  }

  nsCOMPtr<nsIDOMElement> upload;
  nsMetricsUtils::CreateElement(doc, NS_LITERAL_STRING("upload"),
                                getter_AddRefs(upload));
  NS_ENSURE_STATE(upload);

  nsString intervalStr;
  AppendInt(intervalStr, mUploadInterval);
  upload->SetAttribute(NS_LITERAL_STRING("interval"), intervalStr);
  config->AppendChild(upload, getter_AddRefs(nodeOut));

  response->AppendChild(config, getter_AddRefs(nodeOut));

  nsCOMPtr<nsIDOMSerializer> ds =
    do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID);
  NS_ENSURE_STATE(ds);

  nsString docText;
  rv = ds->SerializeToString(response, docText);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF16toUTF8 utf8Doc(docText);
  PRInt32 num = utf8Doc.Length();

  PRFileDesc *fd;
  rv = file->OpenNSPRFileDesc(
      PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool success = (PR_Write(fd, utf8Doc.get(), num) == num);
  PR_Close(fd);

  return success ? NS_OK : NS_ERROR_FAILURE;
}

void
nsMetricsConfig::ForEachChildElement(nsIDOMElement *elem,
                                     ForEachChildElementCallback callback)
{
  nsCOMPtr<nsIDOMNode> node, next;
  elem->GetFirstChild(getter_AddRefs(node));
  while (node) {
    nsCOMPtr<nsIDOMElement> childElem = do_QueryInterface(node);
    if (childElem) {
      // Skip elements that are not in our namespace
      nsString namespaceURI;
      childElem->GetNamespaceURI(namespaceURI);
      if (namespaceURI.Equals(NS_LITERAL_STRING(NS_METRICS_NAMESPACE)))
        (this->*callback)(childElem);
    }
    node->GetNextSibling(getter_AddRefs(next));
    node.swap(next);
  }
}

void
nsMetricsConfig::ProcessToplevelElement(nsIDOMElement *elem)
{
  // Process a top-level element

  nsString name;
  elem->GetLocalName(name);
  if (name.Equals(NS_LITERAL_STRING("config"))) {
    mHasConfig = PR_TRUE;
    ForEachChildElement(elem, &nsMetricsConfig::ProcessConfigChild);
  }
}

void
nsMetricsConfig::ProcessConfigChild(nsIDOMElement *elem)
{
  // Process a config element child

  nsString name;
  elem->GetLocalName(name);
  if (name.Equals(NS_LITERAL_STRING("collectors"))) {
    // Enumerate <collector> elements
    ForEachChildElement(elem, &nsMetricsConfig::ProcessCollectorElement);
  } else if (name.Equals(NS_LITERAL_STRING("limit"))) {
    ReadIntegerAttr(elem, NS_LITERAL_STRING("events"), &mEventLimit);
  } else if (name.Equals(NS_LITERAL_STRING("upload"))) {
    ReadIntegerAttr(elem, NS_LITERAL_STRING("interval"), &mUploadInterval);
  }
}

void
nsMetricsConfig::ProcessCollectorElement(nsIDOMElement *elem)
{
  // Make sure we are dealing with a <collector> element.
  nsString localName;
  elem->GetLocalName(localName);
  if (!localName.Equals(NS_LITERAL_STRING("collector")))
    return;

  nsString type;
  elem->GetAttribute(NS_LITERAL_STRING("type"), type);
  if (type.IsEmpty())
    return;

  // Get the namespace URI specified by any prefix of |type|.
  nsCOMPtr<nsIDOM3Node> node = do_QueryInterface(elem);
  if (!node)
    return;

  // Check to see if this type references a specific namespace.
  PRInt32 colon = FindChar(type, ':');

  nsString namespaceURI;
  if (colon == -1) {
    node->LookupNamespaceURI(EmptyString(), namespaceURI);
    // value is the EventName
  } else {
    // value is NamespacePrefix + ":" + EventName
    nsString prefix(StringHead(type, colon));
    node->LookupNamespaceURI(prefix, namespaceURI);
    type.Cut(0, colon + 1);

    // Add this namespace -> prefix mapping to our lookup table
    mNSURIToPrefixMap.Put(namespaceURI, prefix);
  }

  mEventSet.PutEntry(MakeKey(namespaceURI, type));
}

PRBool
nsMetricsConfig::IsEventEnabled(const nsAString &eventNS,
                                const nsAString &eventName) const
{
  NS_ASSERTION(mEventSet.IsInitialized(), "nsMetricsConfig::Init not called");
  return mEventSet.GetEntry(MakeKey(eventNS, eventName)) != nsnull;
}

void
nsMetricsConfig::SetEventEnabled(const nsAString &eventNS,
                                 const nsAString &eventName, PRBool enabled)
{
  NS_ASSERTION(mEventSet.IsInitialized(), "nsMetricsConfig::Init not called");
  nsString key = MakeKey(eventNS, eventName);
  if (enabled) {
    mEventSet.PutEntry(key);
  } else {
    mEventSet.RemoveEntry(key);
  }
}

void
nsMetricsConfig::ClearEvents()
{
  NS_ASSERTION(mEventSet.IsInitialized(), "nsMetricsConfig::Init not called");
  mEventSet.Clear();
}

/* static */ PLDHashOperator
nsMetricsConfig::CopyKey(nsStringHashKey *entry, void *userData)
{
  static_cast<nsTArray<nsString> *>(userData)->
    AppendElement(entry->GetKey());
  return PL_DHASH_NEXT;
}

void
nsMetricsConfig::GetEvents(nsTArray<nsString> &events) {
  mEventSet.EnumerateEntries(CopyKey, &events);
}
