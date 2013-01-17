/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessibleWrap.h"

#include "nsEventShell.h"

#include "mozilla/StaticPtr.h"

using namespace mozilla;
using namespace mozilla::a11y;

StaticRefPtr<Accessible> HyperTextAccessibleWrap::sLastTextChangeAcc;
StaticAutoPtr<nsString> HyperTextAccessibleWrap::sLastTextChangeString;
uint32_t HyperTextAccessibleWrap::sLastTextChangeStart = 0;
uint32_t HyperTextAccessibleWrap::sLastTextChangeEnd = 0;
bool HyperTextAccessibleWrap::sLastTextChangeWasInsert = false;

NS_IMPL_ISUPPORTS_INHERITED0(HyperTextAccessibleWrap,
                             HyperTextAccessible)

STDMETHODIMP
HyperTextAccessibleWrap::QueryInterface(REFIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr)
    return E_FAIL;

  *aInstancePtr = NULL;

  if (IsTextRole()) {
    if (aIID == IID_IAccessibleText)
      *aInstancePtr =
        static_cast<IAccessibleText*>(static_cast<ia2AccessibleText*>(this));
    else if (aIID == IID_IAccessibleHypertext)
      *aInstancePtr = static_cast<IAccessibleHypertext*>(this);
    else if (aIID == IID_IAccessibleEditableText)
      *aInstancePtr = static_cast<IAccessibleEditableText*>(this);

    if (*aInstancePtr) {
      AddRef();
      return S_OK;
    }
  }

  return AccessibleWrap::QueryInterface(aIID, aInstancePtr);
}

nsresult
HyperTextAccessibleWrap::HandleAccEvent(AccEvent* aEvent)
{
  uint32_t eventType = aEvent->GetEventType();

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_REMOVED ||
      eventType == nsIAccessibleEvent::EVENT_TEXT_INSERTED) {
    Accessible* accessible = aEvent->GetAccessible();
    if (accessible && accessible->IsHyperText()) {
      sLastTextChangeAcc = accessible;
      if (!sLastTextChangeString)
        sLastTextChangeString = new nsString();

      AccTextChangeEvent* event = downcast_accEvent(aEvent);
      event->GetModifiedText(*sLastTextChangeString);
      sLastTextChangeStart = event->GetStartOffset();
      sLastTextChangeEnd = sLastTextChangeStart + event->GetLength();
      sLastTextChangeWasInsert = event->IsTextInserted();
    }
  }

  return HyperTextAccessible::HandleAccEvent(aEvent);
}

nsresult
HyperTextAccessibleWrap::GetModifiedText(bool aGetInsertedText,
                                         nsAString& aText,
                                         uint32_t* aStartOffset,
                                         uint32_t* aEndOffset)
{
  aText.Truncate();
  *aStartOffset = 0;
  *aEndOffset = 0;

  if (!sLastTextChangeAcc)
    return NS_OK;

  if (aGetInsertedText != sLastTextChangeWasInsert)
    return NS_OK;

  if (sLastTextChangeAcc != this)
    return NS_OK;

  *aStartOffset = sLastTextChangeStart;
  *aEndOffset = sLastTextChangeEnd;
  aText.Append(*sLastTextChangeString);

  return NS_OK;
}

