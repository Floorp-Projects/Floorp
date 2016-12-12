/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Unused.h"
#include "nsIObserver.h"
#include "PrintProgressDialogChild.h"

class nsIWebProgress;
class nsIRequest;

using mozilla::Unused;

namespace mozilla {
namespace embedding {

NS_IMPL_ISUPPORTS(PrintProgressDialogChild,
                  nsIWebProgressListener,
                  nsIPrintProgressParams)

PrintProgressDialogChild::PrintProgressDialogChild(
  nsIObserver* aOpenObserver) :
  mOpenObserver(aOpenObserver)
{
  MOZ_COUNT_CTOR(PrintProgressDialogChild);
}

PrintProgressDialogChild::~PrintProgressDialogChild()
{
  // When the printing engine stops supplying information about printing
  // progress, it'll drop references to us and destroy us. We need to signal
  // the parent to decrement its refcount, as well as prevent it from attempting
  // to contact us further.
  Unused << Send__delete__(this);
  MOZ_COUNT_DTOR(PrintProgressDialogChild);
}

mozilla::ipc::IPCResult
PrintProgressDialogChild::RecvDialogOpened()
{
  // nsPrintEngine's observer, which we're reporting to here, doesn't care
  // what gets passed as the subject, topic or data, so we'll just send
  // nullptrs.
  mOpenObserver->Observe(nullptr, nullptr, nullptr);
  return IPC_OK();
}

// nsIWebProgressListener

NS_IMETHODIMP
PrintProgressDialogChild::OnStateChange(nsIWebProgress* aProgress,
                                        nsIRequest* aRequest,
                                        uint32_t aStateFlags,
                                        nsresult aStatus)
{
  Unused << SendStateChange(aStateFlags, aStatus);
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::OnProgressChange(nsIWebProgress * aProgress,
                                           nsIRequest * aRequest,
                                           int32_t aCurSelfProgress,
                                           int32_t aMaxSelfProgress,
                                           int32_t aCurTotalProgress,
                                           int32_t aMaxTotalProgress)
{
  Unused << SendProgressChange(aCurSelfProgress, aMaxSelfProgress,
                               aCurTotalProgress, aMaxTotalProgress);
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::OnLocationChange(nsIWebProgress* aProgress,
                                           nsIRequest* aRequest,
                                           nsIURI* aURI,
                                           uint32_t aFlags)
{
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::OnStatusChange(nsIWebProgress* aProgress,
                                         nsIRequest* aRequest,
                                         nsresult aStatus,
                                         const char16_t* aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::OnSecurityChange(nsIWebProgress* aProgress,
                                           nsIRequest* aRequest,
                                           uint32_t aState)
{
  return NS_OK;
}

// nsIPrintProgressParams

NS_IMETHODIMP PrintProgressDialogChild::GetDocTitle(char16_t* *aDocTitle)
{
  NS_ENSURE_ARG(aDocTitle);

  *aDocTitle = ToNewUnicode(mDocTitle);
  return NS_OK;
}

NS_IMETHODIMP PrintProgressDialogChild::SetDocTitle(const char16_t* aDocTitle)
{
  mDocTitle = aDocTitle;
  Unused << SendDocTitleChange(nsString(aDocTitle));
  return NS_OK;
}

NS_IMETHODIMP PrintProgressDialogChild::GetDocURL(char16_t **aDocURL)
{
  NS_ENSURE_ARG(aDocURL);

  *aDocURL = ToNewUnicode(mDocURL);
  return NS_OK;
}

NS_IMETHODIMP PrintProgressDialogChild::SetDocURL(const char16_t* aDocURL)
{
  mDocURL = aDocURL;
  Unused << SendDocURLChange(nsString(aDocURL));
  return NS_OK;
}

} // namespace embedding
} // namespace mozilla
