/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputPortData_h
#define mozilla_dom_InputPortData_h

#include "mozilla/dom/InputPortBinding.h"
#include "nsIInputPortService.h"

class nsString;

namespace mozilla {
namespace dom {

enum class InputPortType : uint32_t
{
  Av,
  Displayport,
  Hdmi,
  EndGuard_
};

class InputPortData final : public nsIInputPortData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTPORTDATA

  InputPortData();

  const nsString& GetId() const;

  const InputPortType GetType() const;

private:
  ~InputPortData();

  nsString mId;
  nsString mType;
  bool mIsConnected;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InputPortData_h
