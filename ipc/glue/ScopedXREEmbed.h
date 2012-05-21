/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_SCOPEDXREEMBED_H__
#define __IPC_GLUE_SCOPEDXREEMBED_H__

namespace mozilla {
namespace ipc {

class ScopedXREEmbed
{
public:
  ScopedXREEmbed();
  ~ScopedXREEmbed();

  void Start();
  void Stop();

private:
  bool mShouldKillEmbedding;
};

} /* namespace ipc */
} /* namespace mozilla */

#endif /* __IPC_GLUE_SCOPEDXREEMBED_H__ */