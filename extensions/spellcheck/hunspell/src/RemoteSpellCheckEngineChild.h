/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RemoteSpellcheckEngineChild_h_
#define RemoteSpellcheckEngineChild_h_

#include "mozilla/PRemoteSpellcheckEngineChild.h"

namespace mozilla {
class RemoteSpellcheckEngineChild : public mozilla::PRemoteSpellcheckEngineChild
{
public:
  RemoteSpellcheckEngineChild();

  ~RemoteSpellcheckEngineChild();
};

} //namespace mozilla

#endif // RemoteSpellcheckEngineChild_h_
