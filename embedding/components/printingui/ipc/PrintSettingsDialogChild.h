/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_embedding_PrintSettingsDialogChild_h
#define mozilla_embedding_PrintSettingsDialogChild_h

#include "mozilla/embedding/PPrintSettingsDialogChild.h"
namespace mozilla {
namespace embedding {

class PrintSettingsDialogChild final : public PPrintSettingsDialogChild
{
  NS_INLINE_DECL_REFCOUNTING(PrintSettingsDialogChild)

public:
  MOZ_IMPLICIT PrintSettingsDialogChild();

  virtual bool Recv__delete__(const PrintDataOrNSResult& aData) override;

  bool returned() { return mReturned; };
  nsresult result() { return mResult; };
  PrintData data() { return mData; };

private:
  virtual ~PrintSettingsDialogChild();
  bool mReturned;
  nsresult mResult;
  PrintData mData;
};

} // namespace embedding
} // namespace mozilla

#endif
