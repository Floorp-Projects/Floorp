/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScopedXREEmbed.h"

#include "base/command_line.h"
#include "base/string_util.h"

#include "nsIFile.h"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsXULAppAPI.h"

using mozilla::ipc::ScopedXREEmbed;

ScopedXREEmbed::ScopedXREEmbed() : mShouldKillEmbedding(false) { NS_LogInit(); }

ScopedXREEmbed::~ScopedXREEmbed() {
  Stop();
  NS_LogTerm();
}

void ScopedXREEmbed::SetAppDir(const nsACString& aPath) {
  bool flag;
  nsresult rv =
      XRE_GetFileFromPath(aPath.BeginReading(), getter_AddRefs(mAppDir));
  if (NS_FAILED(rv) || NS_FAILED(mAppDir->Exists(&flag)) || !flag) {
    NS_WARNING("Invalid application directory passed to content process.");
    mAppDir = nullptr;
  }
}

void ScopedXREEmbed::Start() {
  nsCOMPtr<nsIFile> localFile;
  nsresult rv = XRE_GetBinaryPath(getter_AddRefs(localFile));
  if (NS_FAILED(rv)) return;

  nsCOMPtr<nsIFile> parent;
  rv = localFile->GetParent(getter_AddRefs(parent));
  if (NS_FAILED(rv)) return;

  localFile = parent;
  NS_ENSURE_TRUE_VOID(localFile);

#ifdef OS_MACOSX
  if (XRE_IsContentProcess()) {
    // We're an XPCOM-using subprocess.  Walk out of
    // [subprocess].app/Contents/MacOS to the real GRE dir.
    rv = localFile->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return;

    localFile = parent;
    NS_ENSURE_TRUE_VOID(localFile);

    rv = localFile->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return;

    localFile = parent;
    NS_ENSURE_TRUE_VOID(localFile);

    rv = localFile->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return;

    localFile = parent;
    NS_ENSURE_TRUE_VOID(localFile);

    rv = localFile->SetNativeLeafName("Resources"_ns);
    if (NS_FAILED(rv)) {
      return;
    }
  }
#endif

  if (mAppDir)
    rv = XRE_InitEmbedding2(localFile, mAppDir, nullptr);
  else
    rv = XRE_InitEmbedding2(localFile, localFile, nullptr);
  if (NS_FAILED(rv)) return;

  mShouldKillEmbedding = true;
}

void ScopedXREEmbed::Stop() {
  if (mShouldKillEmbedding) {
    XRE_TermEmbedding();
    mShouldKillEmbedding = false;
  }
}
