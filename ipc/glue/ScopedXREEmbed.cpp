/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScopedXREEmbed.h"

#include "base/command_line.h"
#include "base/string_util.h"

#include "nsIFile.h"
#include "nsILocalFile.h"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsXULAppAPI.h"

using mozilla::ipc::ScopedXREEmbed;

ScopedXREEmbed::ScopedXREEmbed()
: mShouldKillEmbedding(false)
{
  NS_LogInit();
}

ScopedXREEmbed::~ScopedXREEmbed()
{
  Stop();
  NS_LogTerm();
}

void
ScopedXREEmbed::Start()
{
  std::string path;
#if defined(OS_WIN)
  path = WideToUTF8(CommandLine::ForCurrentProcess()->program());
#elif defined(OS_POSIX)
  path = CommandLine::ForCurrentProcess()->argv()[0];
#else
#  error Sorry
#endif

  nsCOMPtr<nsILocalFile> localFile;
  nsresult rv = XRE_GetBinaryPath(path.c_str(), getter_AddRefs(localFile));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIFile> parent;
  rv = localFile->GetParent(getter_AddRefs(parent));
  if (NS_FAILED(rv))
    return;

  localFile = do_QueryInterface(parent);
  NS_ENSURE_TRUE(localFile,);

#ifdef OS_MACOSX
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // We're an XPCOM-using subprocess.  Walk out of
    // [subprocess].app/Contents/MacOS to the real GRE dir.
    rv = localFile->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv))
      return;

    localFile = do_QueryInterface(parent);
    NS_ENSURE_TRUE(localFile,);

    rv = localFile->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv))
      return;

    localFile = do_QueryInterface(parent);
    NS_ENSURE_TRUE(localFile,);

    rv = localFile->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv))
      return;

    localFile = do_QueryInterface(parent);
    NS_ENSURE_TRUE(localFile,);
  }
#endif

  rv = XRE_InitEmbedding2(localFile, localFile, nsnull);
  if (NS_FAILED(rv))
    return;

  mShouldKillEmbedding = true;
}

void
ScopedXREEmbed::Stop()
{
  if (mShouldKillEmbedding) {
    XRE_TermEmbedding();
    mShouldKillEmbedding = false;
  }
}
