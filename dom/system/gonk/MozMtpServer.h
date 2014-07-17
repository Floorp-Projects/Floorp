/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_mozmtpserver_h__
#define mozilla_system_mozmtpserver_h__

#include "MozMtpCommon.h"

#include "nsCOMPtr.h"
#include "nsIThread.h"

BEGIN_MTP_NAMESPACE

class MozMtpServer
{
public:
  NS_INLINE_DECL_REFCOUNTING(MozMtpServer)

  void Run();

private:
  nsCOMPtr<nsIThread> mServerThread;
};

END_MTP_NAMESPACE

#endif  // mozilla_system_mozmtpserver_h__


