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

// This must be before any #includes to enable logging in release builds
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nsUICommandCollector.h"
#include "nsMetricsService.h"

#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMXULCommandEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMWindow.h"
#include "nsDataHashtable.h"

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

  nsIDOMEventListener* listener = NS_STATIC_CAST(nsIDOMEventListener*,
                                                 userArg);
  if (!listener) {
    MS_LOG(("no event listener in userArg"));
    return PL_DHASH_NEXT;
  }

  nsresult rv = windowTarget->AddEventListener(NS_LITERAL_STRING("command"),
                                               listener, PR_TRUE);
  if (NS_FAILED(rv)) {
    MS_LOG(("Warning: Adding event listener failed, window %p (id %d)",
            key, windowID));
  }
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

  nsIDOMEventListener* listener = NS_STATIC_CAST(nsIDOMEventListener*,
                                                 userArg);
  if (!listener) {
    MS_LOG(("no event listener in userArg"));
    return PL_DHASH_NEXT;
  }

  nsresult rv = windowTarget->RemoveEventListener(NS_LITERAL_STRING("command"),
                                                  listener, PR_TRUE);
  if (NS_FAILED(rv)) {
    MS_LOG(("Warning: Removing event listener failed, window %p (id %d)",
            key, windowID));
  }
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
    // Attach a capturing command listener to the window.
    // Use capturing instead of bubbling so that we still record
    // the event even if propogation is canceled for some reason.
    nsCOMPtr<nsIDOMEventTarget> window = do_QueryInterface(subject);
    NS_ENSURE_STATE(window);

    nsresult rv = window->AddEventListener(NS_LITERAL_STRING("command"),
                                           this,
                                           PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// nsIDomEventListener
NS_IMETHODIMP
nsUICommandCollector::HandleEvent(nsIDOMEvent* event)
{
  // First check that this is an event type that we expect
  nsString type;
  nsresult rv = event->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!type.Equals(NS_LITERAL_STRING("command"))) {

    MS_LOG(("UICommandCollector: Unexpected event type %s received",
            NS_ConvertUTF16toUTF8(type).get()));

    return NS_ERROR_UNEXPECTED;
  }

  // Get the source event for the command.  This will give us the original
  // event in the case where a new event was dispatched due to a command=
  // attribute.
  nsCOMPtr<nsIDOMEvent> sourceEvent;
  nsCOMPtr<nsIDOMXULCommandEvent> commandEvent = do_QueryInterface(event);
  if (commandEvent) {  // nsIDOMXULCommandEvent is only in Gecko 1.8.1+
    commandEvent->GetSourceEvent(getter_AddRefs(sourceEvent));
  }
  if (!sourceEvent) {
    sourceEvent = event;
  }

  // Get the Original Target id - this is the target after text node
  // retargeting.  If this id is blank it means the target is anonymous
  // content.
  nsCOMPtr<nsIDOMNSEvent> nsEvent = do_QueryInterface(sourceEvent);
  NS_ENSURE_STATE(nsEvent);

  nsCOMPtr<nsIDOMEventTarget> original_target;
  nsEvent->GetOriginalTarget(getter_AddRefs(original_target));
  NS_ENSURE_STATE(original_target);

  nsCOMPtr<nsIDOMElement> origElement(do_QueryInterface(original_target));
  nsString orig_id;
  nsString orig_anon;
  if (origElement) {
    origElement->GetAttribute(NS_LITERAL_STRING("id"), orig_id);
    origElement->GetAttribute(NS_LITERAL_STRING("anonid"), orig_anon);
  }

  // Get the target id - this is the target after all retargeting.
  // In the case of anonymous content, the original target ID will
  // be blank and the target ID will be set.
  nsCOMPtr<nsIDOMEventTarget> target;
  sourceEvent->GetTarget(getter_AddRefs(target));

  nsString tar_id;

  nsCOMPtr<nsIDOMElement> tarElement(do_QueryInterface(target));
  if (tarElement) {
    tarElement->GetAttribute(NS_LITERAL_STRING("id"), tar_id);
  }

  MS_LOG(("Original Target Id: %s, Target Id: %s, Anonid: %s",
          NS_ConvertUTF16toUTF8(orig_id).get(),
          NS_ConvertUTF16toUTF8(tar_id).get(),
          NS_ConvertUTF16toUTF8(orig_anon).get()));

  // If the Target Id is empty, return without logging and print an error
  if (tar_id.IsEmpty()) {
    MS_LOG(("Warning: skipping logging because of empty target ID"));
    return NS_OK;
  }

  // If the Original ID (target after text node retargeting) is empty,
  // then assume we are dealing with anonymous content.  In that case,
  // return without logging and print an error if the anonid is empty.
  PRBool logAnonId = PR_FALSE;
  if (orig_id.IsEmpty()) {
    logAnonId = PR_TRUE;

    if (orig_anon.IsEmpty()) {
      MS_LOG(("Warning: skipping logging because of empty anonid"));
      return NS_OK;
    }
  }

  // Get the window that this target is in, so that we can log the event
  // with the appropriate window id.
  nsCOMPtr<nsIDOMNode> targetNode = do_QueryInterface(target);
  if (!targetNode) {
    MS_LOG(("Warning: skipping logging because target is not a node"));
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDocument> ownerDoc;
  targetNode->GetOwnerDocument(getter_AddRefs(ownerDoc));
  NS_ENSURE_STATE(ownerDoc);

  nsCOMPtr<nsIDOMDocumentView> docView = do_QueryInterface(ownerDoc);
  NS_ENSURE_STATE(docView);

  nsCOMPtr<nsIDOMAbstractView> absView;
  docView->GetDefaultView(getter_AddRefs(absView));
  NS_ENSURE_STATE(absView);

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(absView);
  NS_ENSURE_STATE(window);

  // Fill a property bag with what we want to log
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  rv = properties->SetPropertyAsUint32(NS_LITERAL_STRING("window"),
                                       nsMetricsService::GetWindowID(window));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = properties->SetPropertyAsAString(NS_LITERAL_STRING("action"),
                                        type);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the metrics service now so we can use it to hash the ids below
  nsMetricsService *ms = nsMetricsService::get();
  NS_ENSURE_STATE(ms);

  // Log the Target Id which will be the same as the Original Target Id
  // unless the target is anonymous content
  nsCString hashedTarId;
  rv = ms->HashUTF16(tar_id, hashedTarId);
  NS_ENSURE_SUCCESS(rv, rv);
 
  rv = properties->SetPropertyAsACString(NS_LITERAL_STRING("targetidhash"),
                                        hashedTarId);
  NS_ENSURE_SUCCESS(rv, rv);

  if (logAnonId) {
    nsCString hashedAnonId;
    rv = ms->HashUTF16(orig_anon, hashedAnonId);
    NS_ENSURE_SUCCESS(rv, rv);
  
    rv = properties->SetPropertyAsACString(
    NS_LITERAL_STRING("targetanonidhash"), hashedAnonId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Actually log it
  rv = ms->LogEvent(NS_LITERAL_STRING("uielement"), properties);
  NS_ENSURE_SUCCESS(rv, rv);

  MS_LOG(("Successfully logged UI Event"));
  return NS_OK;
}
