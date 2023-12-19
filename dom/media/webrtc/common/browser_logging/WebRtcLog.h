/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTCLOG_H_
#define WEBRTCLOG_H_

#include "nsISupports.h"
#include "nsStringFwd.h"

nsCString StartAecLog();
void StopAecLog();

class WebrtcLogSinkHandle {
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

 protected:
  virtual ~WebrtcLogSinkHandle() = default;
};

already_AddRefed<WebrtcLogSinkHandle> EnsureWebrtcLogging();

#endif
