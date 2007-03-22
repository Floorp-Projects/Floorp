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
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsAutoCompleteCollector.h"
#include "nsMetricsService.h"

#include "nsIObserverService.h"
#include "nsIAutoCompleteController.h"
#include "nsIAutoCompleteInput.h"
#include "nsIAutoCompletePopup.h"
#include "nsIDOMElement.h"
#include "nsServiceManagerUtils.h"

static const char kAutoCompleteTopic[] = "autocomplete-will-enter-text";

nsAutoCompleteCollector::nsAutoCompleteCollector()
{
}

nsAutoCompleteCollector::~nsAutoCompleteCollector()
{
}

NS_IMPL_ISUPPORTS2(nsAutoCompleteCollector, nsIMetricsCollector, nsIObserver)

NS_IMETHODIMP
nsAutoCompleteCollector::OnAttach()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_STATE(obsSvc);

  nsresult rv = obsSvc->AddObserver(this, kAutoCompleteTopic, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteCollector::OnDetach()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_STATE(obsSvc);

  nsresult rv = obsSvc->RemoveObserver(this, kAutoCompleteTopic);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteCollector::OnNewLog()
{
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteCollector::Observe(nsISupports *subject,
                                 const char *topic,
                                 const PRUnichar *data)
{
  if (strcmp(topic, kAutoCompleteTopic) != 0) {
    MS_LOG(("Unexpected observer notification received: %s", topic));
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIAutoCompleteInput> input = do_QueryInterface(subject);
  if (!input) {
    MS_LOG(("subject isn't an AutoCompleteInput"));
    return NS_OK;
  }

  nsCOMPtr<nsIAutoCompletePopup> popup;
  input->GetPopup(getter_AddRefs(popup));
  if (!popup) {
    MS_LOG(("AutoCompleteInput has no popup"));
    return NS_OK;
  }

  PRBool open;
  nsresult rv = popup->GetPopupOpen(&open);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!open) {
    MS_LOG(("AutoComplete popup is closed, not logging"));
    return NS_OK;
  }

  PRInt32 selectedIndex;
  rv = popup->GetSelectedIndex(&selectedIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  if (selectedIndex == -1) {
    MS_LOG(("popup has no selected index, not logging"));
    return NS_OK;
  }

  nsString textValue;
  rv = input->GetTextValue(textValue);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAutoCompleteController> controller;
  input->GetController(getter_AddRefs(controller));
  NS_ENSURE_STATE(controller);

  nsString completion;
  rv = controller->GetValueAt(selectedIndex, completion);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(subject);
  if (!element) {
    MS_LOG(("subject isn't a DOMElement"));
    return NS_OK;
  }

  nsString id;
  element->GetAttribute(NS_LITERAL_STRING("id"), id);
  if (id.IsEmpty()) {
    MS_LOG(("Warning: skipping logging because of empty target ID"));
    return NS_OK;
  }

  // Fill a property bag for the <uielement> item
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  PRInt32 window = nsMetricsUtils::FindWindowForNode(element);
  rv = properties->SetPropertyAsUint32(NS_LITERAL_STRING("window"), window);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = properties->SetPropertyAsAString(NS_LITERAL_STRING("action"),
                                        NS_LITERAL_STRING("autocomplete"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsMetricsService *ms = nsMetricsService::get();
  NS_ENSURE_STATE(ms);

  nsCString hashedId;
  rv = ms->HashUTF16(id, hashedId);
  NS_ENSURE_SUCCESS(rv, rv);
 
  rv = properties->SetPropertyAsACString(NS_LITERAL_STRING("targetidhash"),
                                        hashedId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMetricsEventItem> item;
  ms->CreateEventItem(NS_LITERAL_STRING("uielement"), getter_AddRefs(item));
  NS_ENSURE_STATE(item);
  item->SetProperties(properties);

  // Now fill in the properties for the <autocomplete> child item
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  rv = properties->SetPropertyAsUint32(NS_LITERAL_STRING("typedlength"),
                                      textValue.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = properties->SetPropertyAsInt32(NS_LITERAL_STRING("selectedindex"),
                                      selectedIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = properties->SetPropertyAsInt32(NS_LITERAL_STRING("completedlength"),
                                      completion.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsMetricsUtils::AddChildItem(item, NS_LITERAL_STRING("autocomplete"),
                                    properties);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ms->LogEvent(item);
  NS_ENSURE_SUCCESS(rv, rv);

  MS_LOG(("Logged autocomplete event:\n"
          "  window id: %d\n"
          "  target %s (hash=%s)\n"
          "  typedlength: %d\n"
          "  selectedindex: %d\n"
          "  completedlength: %d",
          window, NS_ConvertUTF16toUTF8(id).get(), hashedId.get(),
          textValue.Length(), selectedIndex, completion.Length()));

  return NS_OK;
}
