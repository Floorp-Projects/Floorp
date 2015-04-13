/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintSettingsDialogChild.h"

using mozilla::unused;

namespace mozilla {
namespace embedding {

PrintSettingsDialogChild::PrintSettingsDialogChild()
: mReturned(false)
{
  MOZ_COUNT_CTOR(PrintSettingsDialogChild);
}

PrintSettingsDialogChild::~PrintSettingsDialogChild()
{
  MOZ_COUNT_DTOR(PrintSettingsDialogChild);
}

bool
PrintSettingsDialogChild::Recv__delete__(const nsresult& aResult,
                                         const PrintData& aData)
{
  mResult = aResult;
  mData = aData;
  mReturned = true;
  return true;
}

} // namespace embedding
} // namespace mozilla
