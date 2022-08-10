/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_dom_media_ipc_RDDProcessImpl_h__
#define _include_dom_media_ipc_RDDProcessImpl_h__
#include "mozilla/ipc/ProcessChild.h"

#if defined(XP_WIN)
#  include "mozilla/mscom/ProcessRuntime.h"
#endif

#include "RDDParent.h"

namespace mozilla {

// This class owns the subprocess instance of a PRDD - which in this case,
// is a RDDParent. It is instantiated as a singleton in XRE_InitChildProcess.
class RDDProcessImpl final : public ipc::ProcessChild {
 public:
  using ipc::ProcessChild::ProcessChild;
  ~RDDProcessImpl();

  bool Init(int aArgc, char* aArgv[]) override;
  void CleanUp() override;

 private:
  RDDParent mRDD;

#if defined(XP_WIN)
  // This object initializes and configures COM.
  mozilla::mscom::ProcessRuntime mCOMRuntime;
#endif
};

}  // namespace mozilla

#endif  // _include_dom_media_ipc_RDDProcessImpl_h__
