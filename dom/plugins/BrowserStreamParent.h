/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */

#ifndef mozilla_plugins_BrowserStreamParent_h
#define mozilla_plugins_BrowserStreamParent_h

#include "mozilla/plugins/PBrowserStreamParent.h"

namespace mozilla {
namespace plugins {

class PluginInstanceParent;

class BrowserStreamParent : public PBrowserStreamParent
{
  friend class PluginModuleParent;
  friend class PluginInstanceParent;

public:
  BrowserStreamParent(PluginInstanceParent* npp,
                      NPStream* stream);
  virtual ~BrowserStreamParent() { }

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
