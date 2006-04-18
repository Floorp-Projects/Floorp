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
#include "nsIFile.h"
#include "nsComponentManagerUtils.h"
#include "nsNetCID.h"
#include "prprf.h"

#define NS_DEFAULT_UPLOAD_INTERVAL 3600  // 1 hour

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
  if (!mEventSet.Init()) {
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
  mEventLimit = PR_INT32_MAX;
  mUploadInterval = NS_DEFAULT_UPLOAD_INTERVAL;
}

nsresult
nsMetricsConfig::Load(nsIFile *file)
{
  // The given file references a XML file with the following structure:
  //
  // <config xmlns="http://www.mozilla.org/metrics"
  //         xmlns:foo="http://foo.com/metrics">
  //   <collectors>
  //     <collector type="ui"/>
  //     <collector type="document"/>
  //     <collector type="window"/>
  //     <collector type="foo:mystat"/>
  //   </collectors>
  //   <limit events="200"/>
  //   <upload interval="600"/>
  // </config>

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

  // At this point, we can clear our old configuration.
  Reset();

  // Now, walk the DOM.  All config elements are optional, and we ignore stuff
  // that we don't understand.
  nsCOMPtr<nsIDOMElement> elem;
  doc->GetDocumentElement(getter_AddRefs(elem));
  if (!elem)
    return NS_OK;

  ForEachChildElement(elem, &nsMetricsConfig::ProcessToplevelElement);
  return NS_OK;
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
    node->LookupNamespaceURI(StringHead(type, colon), namespaceURI);
    type.Cut(0, colon + 1);
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
