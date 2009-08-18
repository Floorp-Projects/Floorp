/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */

#ifndef mozilla_plugins_NPBrowserStreamChild_h
#define mozilla_plugins_NPBrowserStreamChild_h

#include "mozilla/plugins/NPBrowserStreamProtocolChild.h"

namespace mozilla {
namespace plugins {

class NPPInstanceChild;

class NPBrowserStreamChild : public NPBrowserStreamProtocolChild
{
public:
  NPBrowserStreamChild(NPPInstanceChild* instance,
                       const nsCString& url,
                       const uint32_t& length,
                       const uint32_t& lastmodified,
                       const nsCString& headers,
                       const nsCString& mimeType,
                       const bool& seekable,
                       NPError* rv,
                       uint16_t* stype);
  virtual ~NPBrowserStreamChild() { }

  virtual nsresult AnswerNPP_WriteReady(const int32_t& newlength, int32_t *size);
  virtual nsresult AnswerNPP_Write(const int32_t& offset, const Buffer& data,
                                   int32_t* consumed);

  virtual nsresult AnswerNPP_StreamAsFile(const nsCString& fname);

private:
  NPPInstanceChild* mInstance;
  NPStream mStream;
  bool mClosed;
  nsCString mURL;
  nsCString mHeaders;
};

} // namespace plugins
} // namespace mozilla

#endif
