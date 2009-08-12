/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */

#ifndef mozilla_dom_ContentProcessChild_h
#define mozilla_dom_ContentProcessChild_h

#include "mozilla/dom/ContentProcessProtocolChild.h"

namespace mozilla {
namespace dom {

class ContentProcessChild
  : public ContentProcessProtocolChild
{
public:
  ContentProcessChild();
  virtual ~ContentProcessChild();

  bool Init(MessageLoop* aIOLoop, IPC::Channel* aChannel);

  virtual IFrameEmbeddingProtocolChild* IFrameEmbeddingConstructor(const MagicWindowHandle& hwnd);
  virtual nsresult IFrameEmbeddingDestructor(IFrameEmbeddingProtocolChild*);

private:
  DISALLOW_EVIL_CONSTRUCTORS(ContentProcessChild);
};

} // namespace dom
} // namespace mozilla

#endif
