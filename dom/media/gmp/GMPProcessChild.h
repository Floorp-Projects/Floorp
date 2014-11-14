/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPProcessChild_h_
#define GMPProcessChild_h_

#include "mozilla/ipc/ProcessChild.h"
#include "GMPChild.h"

namespace mozilla {
namespace gmp {

class GMPLoader;

class GMPProcessChild MOZ_FINAL : public mozilla::ipc::ProcessChild {
protected:
  typedef mozilla::ipc::ProcessChild ProcessChild;

public:
  explicit GMPProcessChild(ProcessHandle parentHandle);
  ~GMPProcessChild();

  virtual bool Init() MOZ_OVERRIDE;
  virtual void CleanUp() MOZ_OVERRIDE;

  // Set/get the GMPLoader singleton for this child process.
  // Note: The GMPLoader is not deleted by this object, the caller of
  // SetGMPLoader() must manage the GMPLoader's lifecycle.
  static void SetGMPLoader(GMPLoader* aHost);
  static GMPLoader* GetGMPLoader();

private:
  GMPChild mPlugin;
  static GMPLoader* mLoader;
  DISALLOW_COPY_AND_ASSIGN(GMPProcessChild);
};

} // namespace gmp
} // namespace mozilla

#endif // GMPProcessChild_h_
