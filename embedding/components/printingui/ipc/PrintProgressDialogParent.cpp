/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Unused.h"
#include "nsIPrintProgressParams.h"
#include "nsIWebProgressListener.h"
#include "PrintProgressDialogParent.h"

using mozilla::Unused;

namespace mozilla {
namespace embedding {

NS_IMPL_ISUPPORTS(PrintProgressDialogParent, nsIObserver)

PrintProgressDialogParent::PrintProgressDialogParent() :
  mActive(true)
{
  MOZ_COUNT_CTOR(PrintProgressDialogParent);
}

PrintProgressDialogParent::~PrintProgressDialogParent()
{
  MOZ_COUNT_DTOR(PrintProgressDialogParent);
}

void
PrintProgressDialogParent::SetWebProgressListener(nsIWebProgressListener* aListener)
{
  mWebProgressListener = aListener;
}

void
PrintProgressDialogParent::SetPrintProgressParams(nsIPrintProgressParams* aParams)
{
  mPrintProgressParams = aParams;
}

bool
PrintProgressDialogParent::RecvStateChange(const long& stateFlags,
                                           const nsresult& status)
{
  if (mWebProgressListener) {
    mWebProgressListener->OnStateChange(nullptr, nullptr, stateFlags, status);
  }
  return true;
}

bool
PrintProgressDialogParent::RecvProgressChange(const long& curSelfProgress,
                                              const long& maxSelfProgress,
                                              const long& curTotalProgress,
                                              const long& maxTotalProgress)
{
  if (mWebProgressListener) {
    mWebProgressListener->OnProgressChange(nullptr, nullptr, curSelfProgress,
                                           maxSelfProgress, curTotalProgress,
                                           maxTotalProgress);
  }
  return true;
}

bool
PrintProgressDialogParent::RecvDocTitleChange(const nsString& newTitle)
{
  if (mPrintProgressParams) {
    mPrintProgressParams->SetDocTitle(newTitle.get());
  }
  return true;
}

bool
PrintProgressDialogParent::RecvDocURLChange(const nsString& newURL)
{
  if (mPrintProgressParams) {
    mPrintProgressParams->SetDocURL(newURL.get());
  }
  return true;
}

void
PrintProgressDialogParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

bool
PrintProgressDialogParent::Recv__delete__()
{
  // The child has requested that we tear down the connection, so we set a
  // member to make sure we don't try to contact it after the fact.
  mActive = false;
  return true;
}

// nsIObserver
NS_IMETHODIMP
PrintProgressDialogParent::Observe(nsISupports *aSubject, const char *aTopic,
                                   const char16_t *aData)
{
  if (mActive) {
    Unused << SendDialogOpened();
  } else {
    NS_WARNING("The print progress dialog finished opening, but communications "
               "with the child have been closed.");
  }

  return NS_OK;
}


} // namespace embedding
} // namespace mozilla
