/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputPortData.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

namespace {

InputPortType
ToInputPortType(const nsAString& aStr)
{
  if (aStr.EqualsLiteral("av")) {
    return InputPortType::Av;
  }

  if (aStr.EqualsLiteral("displayport")) {
    return InputPortType::Displayport;
  }

  if (aStr.EqualsLiteral("hdmi")) {
    return InputPortType::Hdmi;
  }

  return InputPortType::EndGuard_;
}

} // namespace

NS_IMPL_ISUPPORTS(InputPortData, nsIInputPortData)

InputPortData::InputPortData()
  : mIsConnected(false)
{
}

InputPortData::~InputPortData()
{
}

/* virtual */ NS_IMETHODIMP
InputPortData::GetId(nsAString& aId)
{
  aId = mId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
InputPortData::SetId(const nsAString& aId)
{
  if (aId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mId = aId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
InputPortData::GetType(nsAString& aType)
{
  aType = mType;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
InputPortData::SetType(const nsAString& aType)
{
  if (aType.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (InputPortType::EndGuard_ == ToInputPortType(aType)) {
    return NS_ERROR_INVALID_ARG;
  }

  mType = aType;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
InputPortData::GetConnected(bool* aIsConnected)
{
  *aIsConnected = mIsConnected;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
InputPortData::SetConnected(const bool aIsConnected)
{
  mIsConnected = aIsConnected;
  return NS_OK;
}

const nsString&
InputPortData::GetId() const
{
  return mId;
}

const InputPortType
InputPortData::GetType() const
{
  return ToInputPortType(mType);
}

} // namespace dom
} // namespace mozilla
