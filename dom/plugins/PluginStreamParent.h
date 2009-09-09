/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */

#ifndef mozilla_plugins_NPBrowserStreamParent_h
#define mozilla_plugins_NPBrowserStreamParent_h

#include "mozilla/plugins/PPluginStreamProtocolParent.h"

namespace mozilla {
namespace plugins {

class PluginInstanceParent;

class PluginStreamParent : public PPluginStreamProtocolParent
{
  friend class PluginModuleParent;
  friend class PluginInstanceParent;

public:
  PluginStreamParent(PluginInstanceParent* npp,
                     NPStream* stream);
  virtual ~PluginStreamParent() { }

  virtual nsresult AnswerNPN_RequestRead(const IPCByteRanges& ranges,
                                         NPError* result);

  int32_t WriteReady();
  int32_t Write(int32_t offset, int32_t len, void* buffer);
  void StreamAsFile(const char* fname);
  NPError NPN_DestroyStream(NPError reason);

private:
  PluginInstanceParent* mNPP;
  NPStream* mStream;
};

} // namespace plugins
} // namespace mozilla

#endif
