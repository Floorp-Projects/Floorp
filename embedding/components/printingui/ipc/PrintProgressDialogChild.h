/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_embedding_PrintProgressDialogChild_h
#define mozilla_embedding_PrintProgressDialogChild_h

#include "mozilla/embedding/PPrintProgressDialogChild.h"
#include "nsIPrintProgressParams.h"
#include "nsIWebProgressListener.h"

class nsIObserver;

namespace mozilla {
namespace embedding {

class PrintProgressDialogChild final : public PPrintProgressDialogChild,
                                       public nsIWebProgressListener,
                                       public nsIPrintProgressParams
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIPRINTPROGRESSPARAMS

public:
  MOZ_IMPLICIT PrintProgressDialogChild(nsIObserver* aOpenObserver);

  virtual mozilla::ipc::IPCResult RecvDialogOpened() override;

private:
  virtual ~PrintProgressDialogChild();
  nsCOMPtr<nsIObserver> mOpenObserver;
  nsString mDocTitle;
  nsString mDocURL;
};

} // namespace embedding
} // namespace mozilla

#endif
