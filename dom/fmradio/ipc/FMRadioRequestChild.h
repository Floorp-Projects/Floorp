/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fmradiorequestchild_h__
#define mozilla_dom_fmradiorequestchild_h__

#include "FMRadioCommon.h"
#include "mozilla/dom/PFMRadioRequestChild.h"
#include "DOMRequest.h"

BEGIN_FMRADIO_NAMESPACE

class FMRadioReplyRunnable;

class FMRadioRequestChild final : public PFMRadioRequestChild
{
public:
  FMRadioRequestChild(FMRadioReplyRunnable* aReplyRunnable);
  ~FMRadioRequestChild();

  virtual bool
  Recv__delete__(const FMRadioResponseType& aResponse) override;

private:
  RefPtr<FMRadioReplyRunnable> mReplyRunnable;
};

END_FMRADIO_NAMESPACE

#endif // mozilla_dom_fmradiorequestchild_h__

