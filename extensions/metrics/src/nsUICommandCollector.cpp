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
 *  Marria Nazif <marria@gmail.com>
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

#include "nsUICommandCollector.h"
#include "nsMetricsService.h"

#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMXULCommandEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsDataHashtable.h"
#include "nsMemory.h"
#ifndef MOZ_PLACES_BOOKMARKS
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsIRDFContainer.h"
#include "nsIBookmarksService.h"
#include "nsIArray.h"
#include "nsComponentManagerUtils.h"
#endif

const nsUICommandCollector::EventHandler nsUICommandCollector::kEvents[] = {
  { "command", &nsUICommandCollector::HandleCommandEvent },
  { "TabMove", &nsUICommandCollector::HandleTabMoveEvent },
};

NS_IMPL_ISUPPORTS3(nsUICommandCollector, nsIObserver, nsIDOMEventListener,
                   nsIMetricsCollector)

/* static */
PLDHashOperator PR_CALLBACK nsUICommandCollector::AddCommandEventListener(
const nsIDOMWindow* key, PRUint32 windowID, void* userArg)
{
  nsCOMPtr<nsIDOMEventTarget> windowTarget =
    do_QueryInterface(NS_CONST_CAST(nsIDOMWindow *, key));
  if (!windowTarget) {
    MS_LOG(("Error casting domeventtarget"));
    return PL_DHASH_NEXT;
  }

  nsUICommandCollector* collector = NS_STATIC_CAST(nsUICommandCollector*,
                                                   userArg);
  collector->AddEventListeners(windowTarget);
  return PL_DHASH_NEXT;
}

/* static */
PLDHashOperator PR_CALLBACK nsUICommandCollector::RemoveCommandEventListener(
const nsIDOMWindow* key, PRUint32 windowID, void* userArg)
{
  nsCOMPtr<nsIDOMEventTarget> windowTarget =
    do_QueryInterface(NS_CONST_CAST(nsIDOMWindow *, key));
  if (!windowTarget) {
    MS_LOG(("Error casting domeventtarget"));
    return PL_DHASH_NEXT;
  }

  nsUICommandCollector* collector = NS_STATIC_CAST(nsUICommandCollector*,
                                                   userArg);
  collector->RemoveEventListeners(windowTarget);
  return PL_DHASH_NEXT;
}

nsUICommandCollector::nsUICommandCollector()
{
}

nsUICommandCollector::~nsUICommandCollector()
{
}

// nsIMetricsCollector
NS_IMETHODIMP
nsUICommandCollector::OnAttach()
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_STATE(obsSvc);

  // Listen for newly opened windows, so that we can attach a command event
  // listener to each window
  rv = obsSvc->AddObserver(this, "domwindowopened", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Attach to all existing windows
  nsMetricsService *ms = nsMetricsService::get();
  NS_ENSURE_STATE(ms);

  ms->WindowMap().EnumerateRead(AddCommandEventListener,
                                NS_STATIC_CAST(nsIDOMEventListener*, this));

  return NS_OK;
}

NS_IMETHODIMP
nsUICommandCollector::OnDetach()
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_STATE(obsSvc);

  // Remove our observer for open windows
  rv = obsSvc->RemoveObserver(this, "domwindowopened");
  NS_ENSURE_SUCCESS(rv, rv);

  // Also iterate through all windows and try to remove command event
  // listeners.  It is possible that we never attached one to some
  // of the windows (if we were detached and then attached) so
  // continue on even if it fails
  nsMetricsService *ms = nsMetricsService::get();
  NS_ENSURE_STATE(ms);

  ms->WindowMap().EnumerateRead(RemoveCommandEventListener,
    NS_STATIC_CAST(nsIDOMEventListener*, this));

  return NS_OK;
}

NS_IMETHODIMP
nsUICommandCollector::OnNewLog()
{
  return NS_OK;
}

// nsIObserver
NS_IMETHODIMP
nsUICommandCollector::Observe(nsISupports *subject,
                              const char *topic,
                              const PRUnichar *data)
{
  if (strcmp(topic, "domwindowopened") == 0) {
    nsCOMPtr<nsIDOMEventTarget> window = do_QueryInterface(subject);
    NS_ENSURE_STATE(window);
    AddEventListeners(window);
  }

  return NS_OK;
}

// nsIDOMEventListener
NS_IMETHODIMP
nsUICommandCollector::HandleEvent(nsIDOMEvent* event)
{
  // First check that this is an event type that we expect.
  // If so, call the appropriate handler method.
  nsString type;
  nsresult rv = event->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(kEvents); ++i) {
    if (NS_ConvertASCIItoUTF16(kEvents[i].event).Equals(type)) {
      return (this->*(kEvents[i].handler))(event);
    }
  }

  MS_LOG(("UICommandCollector: Unexpected event type %s received",
          NS_ConvertUTF16toUTF8(type).get()));
  return NS_ERROR_UNEXPECTED;
}

nsresult
nsUICommandCollector::HandleCommandEvent(nsIDOMEvent* event)
{
  PRUint32 window;
  if (NS_FAILED(GetEventWindow(event, &window))) {
    return NS_OK;
  }

  nsString targetId, targetAnonId;
  if (NS_FAILED(GetEventTargets(event, targetId, targetAnonId))) {
    return NS_OK;
  }
  NS_ASSERTION(!targetId.IsEmpty(), "can't have an empty target id");

  nsString keyId;
  GetEventKeyId(event, keyId);

  // Fill a property bag with what we want to log
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  nsresult rv;
  rv = properties->SetPropertyAsUint32(NS_LITERAL_STRING("window"), window);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = properties->SetPropertyAsAString(NS_LITERAL_STRING("action"),
                                        NS_LITERAL_STRING("command"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetHashedValue(properties, NS_LITERAL_STRING("targetidhash"), targetId);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!targetAnonId.IsEmpty()) {
    rv = SetHashedValue(properties, NS_LITERAL_STRING("targetanonidhash"),
                        targetAnonId);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (!keyId.IsEmpty()) {
    rv = SetHashedValue(properties, NS_LITERAL_STRING("keyidhash"), keyId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsMetricsService *ms = nsMetricsService::get();
  NS_ENSURE_STATE(ms);

  nsCOMPtr<nsIMetricsEventItem> item;
  ms->CreateEventItem(NS_LITERAL_STRING("uielement"), getter_AddRefs(item));
  NS_ENSURE_STATE(item);
  item->SetProperties(properties);

  // Capture extra bookmark state onto the event if the target is a bookmark.
  rv = LogBookmarkInfo(targetId, item);
  NS_ENSURE_SUCCESS(rv, rv);

  // Actually log it
  rv = ms->LogEvent(item);
  NS_ENSURE_SUCCESS(rv, rv);

  MS_LOG(("Successfully logged UI Event"));
  return NS_OK;
}

nsresult
nsUICommandCollector::HandleTabMoveEvent(nsIDOMEvent* event)
{
  PRUint32 window;
  if (NS_FAILED(GetEventWindow(event, &window))) {
    return NS_OK;
  }

  // Fill a property bag with what we want to log
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  nsresult rv;
  rv = properties->SetPropertyAsUint32(NS_LITERAL_STRING("window"), window);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = properties->SetPropertyAsAString(NS_LITERAL_STRING("action"),
                                        NS_LITERAL_STRING("comand"));
  NS_ENSURE_SUCCESS(rv, rv);

  // TabMove events just have a dummy target id of "TabMove_Event".
  rv = SetHashedValue(properties, NS_LITERAL_STRING("targetidhash"),
                      NS_LITERAL_STRING("TabMove_Event"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsMetricsService *ms = nsMetricsService::get();
  NS_ENSURE_STATE(ms);

  rv = ms->LogEvent(NS_LITERAL_STRING("uielement"), properties);
  NS_ENSURE_SUCCESS(rv, rv);

  MS_LOG(("Successfully logged UI Event"));
  return NS_OK;
}

nsresult
nsUICommandCollector::GetEventTargets(nsIDOMEvent *event,
                                      nsString &targetId,
                                      nsString &targetAnonId) const
{
  // This code deals with both anonymous and explicit (non-anonymous) content.
  //
  // For explicit content, we just return the id of the event target in
  // targetId, and leave targetAnonId empty.  If there is no id, then
  // we return failure.
  //
  // For anonymous content, we return the id of the event target (after
  // retargeting), in targetId.  We return the anonid of the event's
  // originalTarget in targetAnonId, so that XBL child elements can be
  // distinguished.  If there is no anonid, then we return failure.
  // Note that the originalTarget is set after text node retargting, but
  // before any XBL retargeting.
  //
  // We assume that if the originalTarget has no id, we're dealing with
  // anonymous content (this isn't always true, but it's good enough for what
  // this code does).

  nsCOMPtr<nsIDOMNSEvent> nsEvent = do_QueryInterface(event);
  NS_ENSURE_STATE(nsEvent);

  nsCOMPtr<nsIDOMEventTarget> originalTarget;
  nsEvent->GetOriginalTarget(getter_AddRefs(originalTarget));
  NS_ENSURE_STATE(originalTarget);

  nsString origElementId;
  nsCOMPtr<nsIDOMElement> origElement(do_QueryInterface(originalTarget));
  if (origElement) {
    origElement->GetAttribute(NS_LITERAL_STRING("id"), origElementId);
    origElement->GetAttribute(NS_LITERAL_STRING("anonid"), targetAnonId);
  }

  nsCOMPtr<nsIDOMEventTarget> target;
  event->GetTarget(getter_AddRefs(target));
  NS_ENSURE_STATE(target);

  nsCOMPtr<nsIDOMElement> targetElement(do_QueryInterface(target));
  if (targetElement) {
    targetElement->GetAttribute(NS_LITERAL_STRING("id"), targetId);
  }

  MS_LOG(("Original Target Id: %s, Original Target Anonid: %s, Target Id: %s",
          NS_ConvertUTF16toUTF8(origElementId).get(),
          NS_ConvertUTF16toUTF8(targetAnonId).get(),
          NS_ConvertUTF16toUTF8(targetId).get()));

  if (targetId.IsEmpty()) {
    // There's nothing useful to log in this case -- even if we have an anonid,
    // it's not possible to determine its position in the document.
    MS_LOG(("Warning: skipping logging because of empty target ID"));
    return NS_ERROR_FAILURE;
  }

  if (origElementId.IsEmpty()) {
    // We're dealing with anonymous content, so don't continue if we didn't
    // find an anonid.
    if (targetAnonId.IsEmpty()) {
      MS_LOG(("Warning: skipping logging because of empty anonid"));
      return NS_ERROR_FAILURE;
    }
  } else {
    // We're dealing with normal explicit content, so don't return an anonid.
    targetAnonId.SetLength(0);
  }

  return NS_OK;
}

void
nsUICommandCollector::GetEventKeyId(nsIDOMEvent *event, nsString &keyId) const
{
  // The source event will give us the original event in the case where a new
  // event was dispatched due to a command= attribute.
  nsCOMPtr<nsIDOMXULCommandEvent> commandEvent = do_QueryInterface(event);
  if (!commandEvent) {
    // nsIDOMXULCommandEvent is only in Gecko 1.8.1+
    return;
  }

  nsCOMPtr<nsIDOMEvent> sourceEvent;
  commandEvent->GetSourceEvent(getter_AddRefs(sourceEvent));
  if (!sourceEvent) {
    return;
  }

  nsCOMPtr<nsIDOMEventTarget> sourceEventTarget;
  sourceEvent->GetTarget(getter_AddRefs(sourceEventTarget));
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(sourceEventTarget);
  if (!sourceElement) {
    return;
  }

  nsString namespaceURI, localName;
  sourceElement->GetNamespaceURI(namespaceURI);
  sourceElement->GetLocalName(localName);
  if (namespaceURI.Equals(NS_LITERAL_STRING("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul")) &&
      localName.Equals(NS_LITERAL_STRING("key"))) {
    sourceElement->GetAttribute(NS_LITERAL_STRING("id"), keyId);
    MS_LOG(("Key Id: %s", NS_ConvertUTF16toUTF8(keyId).get()));
  }
}

nsresult
nsUICommandCollector::GetEventWindow(nsIDOMEvent *event,
                                     PRUint32 *window) const
{
  nsCOMPtr<nsIDOMEventTarget> target;
  event->GetTarget(getter_AddRefs(target));
  nsCOMPtr<nsIDOMNode> targetNode = do_QueryInterface(target);
  if (!targetNode) {
    MS_LOG(("Warning: couldn't get window id because target is not a node"));
    return NS_ERROR_FAILURE;
  }

  *window = nsMetricsUtils::FindWindowForNode(targetNode);
  return NS_OK;
}

void
nsUICommandCollector::AddEventListeners(nsIDOMEventTarget *window)
{
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(kEvents); ++i) {
    // Attach a capturing event listener to the window.
    // Use capturing instead of bubbling so that we still record
    // the event even if propagation is canceled for some reason.

    // Using NS_LITERAL_STRING in const data is not allowed, so convert here.
    // This is not performance-sensitive.
    nsresult rv;
    rv = window->AddEventListener(NS_ConvertASCIItoUTF16(kEvents[i].event),
                                  this, PR_TRUE);
    if (NS_FAILED(rv)) {
      MS_LOG(("Couldn't add event listener %s", kEvents[i]));
    }
  }
}

void
nsUICommandCollector::RemoveEventListeners(nsIDOMEventTarget *window)
{
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(kEvents); ++i) {
    // Using NS_LITERAL_STRING in const data is not allowed, so convert here.
    // This is not performance-sensitive.
    nsresult rv;
    rv = window->RemoveEventListener(NS_ConvertASCIItoUTF16(kEvents[i].event),
                                     this, PR_TRUE);
    if (NS_FAILED(rv)) {
      MS_LOG(("Couldn't remove event listener %s", kEvents[i]));
    }
  }
}

/* static */ nsresult
nsUICommandCollector::SetHashedValue(nsIWritablePropertyBag2 *properties,
                                     const nsString &propertyName,
                                     const nsString &propertyValue) const
{
  nsMetricsService *ms = nsMetricsService::get();
  NS_ENSURE_STATE(ms);

  nsCString hashedValue;
  nsresult rv = ms->HashUTF16(propertyValue, hashedValue);
  NS_ENSURE_SUCCESS(rv, rv);

  return properties->SetPropertyAsACString(propertyName, hashedValue);
}

nsresult
nsUICommandCollector::LogBookmarkInfo(const nsString& id,
                                      nsIMetricsEventItem* parentItem)
{
#ifdef MOZ_PLACES_BOOKMARKS
  // TODO: write me!
  // see bug #356606
  return NS_OK;

#else

  // First check whether this is an anonymous RDF id.
  // If it's not, we know it's not a bookmark id at all.
  if (!StringHead(id, strlen("rdf:")).Equals(NS_LITERAL_STRING("rdf:"))) {
    return NS_OK;
  }

  nsCOMPtr<nsIRDFService> rdfSvc =
    do_GetService("@mozilla.org/rdf/rdf-service;1");
  NS_ENSURE_STATE(rdfSvc);

  nsCOMPtr<nsIRDFResource> targetResource;
  rdfSvc->GetUnicodeResource(id, getter_AddRefs(targetResource));
  NS_ENSURE_STATE(targetResource);

  nsCOMPtr<nsIWritablePropertyBag2> bmProps;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(bmProps));
  NS_ENSURE_STATE(bmProps);

  nsCOMPtr<nsIBookmarksService> bmSvc =
    do_GetService(NS_BOOKMARKS_SERVICE_CONTRACTID);
  NS_ENSURE_STATE(bmSvc);

  nsCOMPtr<nsIArray> parentChain;
  bmSvc->GetParentChain(targetResource, getter_AddRefs(parentChain));
  NS_ENSURE_STATE(parentChain);

  PRUint32 depth = 0;
  parentChain->GetLength(&depth);
  bmProps->SetPropertyAsInt32(NS_LITERAL_STRING("depth"), depth);
  if (depth == 0) {
    // Hm, an event on the bookmarks root?  Not much to log in this case.
    return NS_OK;
  }

  nsCOMPtr<nsIRDFDataSource> bmDS =
    do_GetService(NS_BOOKMARKS_DATASOURCE_CONTRACTID);
  NS_ENSURE_STATE(bmDS);

  // Find the bookmark's position in its parent folder.
  // do_QueryElementAt() isn't a frozen export :-(
  nsCOMPtr<nsIRDFResource> parent;
  parentChain->QueryElementAt(depth - 1, NS_GET_IID(nsIRDFResource),
                              getter_AddRefs(parent));
  NS_ENSURE_STATE(parent);

  nsCOMPtr<nsIRDFContainer> container =
    do_CreateInstance("@mozilla.org/rdf/container;1");
  NS_ENSURE_STATE(container);

  nsresult rv = container->Init(bmDS, parent);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 pos;
  rv = container->IndexOf(targetResource, &pos);
  NS_ENSURE_SUCCESS(rv, rv);
  if (pos == -1) {
    MS_LOG(("Bookmark not contained in its parent?"));
  } else {
    bmProps->SetPropertyAsInt32(NS_LITERAL_STRING("pos"), pos);
  }

  // Determine whether the bookmark is under the toolbar folder
  PRBool isToolbarBM = PR_FALSE;
  nsCOMPtr<nsIRDFResource> toolbarFolder;
  bmSvc->GetBookmarksToolbarFolder(getter_AddRefs(toolbarFolder));
  if (toolbarFolder) {
    // Since the user can designate any folder as the toolbar folder,
    // we must walk the entire parent chain looking for it.
    for (PRUint32 i = 0; i < depth; ++i) {
      nsCOMPtr<nsIRDFResource> item;
      parentChain->QueryElementAt(i, NS_GET_IID(nsIRDFResource),
                                  getter_AddRefs(item));
      if (toolbarFolder == item) {
        isToolbarBM = PR_TRUE;
        break;
      }
    }
  }
  bmProps->SetPropertyAsBool(NS_LITERAL_STRING("toolbar"), isToolbarBM);

  return nsMetricsUtils::AddChildItem(parentItem,
                                      NS_LITERAL_STRING("bookmark"), bmProps);
#endif  // MOZ_PLACES_BOOKMARKS
}
