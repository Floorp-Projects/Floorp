/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginStreamChild_h
#define mozilla_plugins_PluginStreamChild_h

#include "mozilla/plugins/PPluginStreamChild.h"
#include "mozilla/plugins/AStream.h"

namespace mozilla {
namespace plugins {

class PluginInstanceChild;

class PluginStreamChild : public PPluginStreamChild, public AStream
{
  friend class PluginInstanceChild;

public:
  PluginStreamChild();
  virtual ~PluginStreamChild() { }

  NS_OVERRIDE virtual bool IsBrowserStream() { return false; }

  virtual bool Answer__delete__(const NPReason& reason,
                                const bool& artificial);

  int32_t NPN_Write(int32_t length, void* buffer);
  void NPP_DestroyStream(NPError reason);

  void EnsureCorrectInstance(PluginInstanceChild* i)
  {
    if (i != Instance())
      NS_RUNTIMEABORT("Incorrect stream instance");
  }
  void EnsureCorrectStream(NPStream* s)
  {
    if (s != &mStream)
      NS_RUNTIMEABORT("Incorrect stream data");
  }

private:
  PluginInstanceChild* Instance();

  NPStream mStream;
  bool mClosed;
};


} // namespace plugins
} // namespace mozilla

#endif
