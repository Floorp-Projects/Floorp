/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */

#ifndef mozilla_plugins_NPBrowserStreamParent_h
#define mozilla_plugins_NPBrowserStreamParent_h

#include "mozilla/plugins/NPBrowserStreamProtocolParent.h"

namespace mozilla {
namespace plugins {

class NPPInstanceParent;

class NPBrowserStreamParent : public NPBrowserStreamProtocolParent
{
  friend class NPAPIPluginParent;
  friend class NPPInstanceParent;

public:
  NPBrowserStreamParent(NPPInstanceParent* npp,
                        NPStream* stream);
  virtual ~NPBrowserStreamParent() { }

  virtual nsresult AnswerNPN_RequestRead(const IPCByteRanges& ranges,
                                         NPError* result);

  int32_t WriteReady();
  int32_t Write(int32_t offset, int32_t len, void* buffer);
  void StreamAsFile(const char* fname);
  NPError NPN_DestroyStream(NPError reason);

private:
  NPPInstanceParent* mNPP;
  NPStream* mStream;
};

} // namespace plugins
} // namespace mozilla

#endif
