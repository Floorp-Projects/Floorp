/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

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

    static ContentProcessChild* GetSingleton() {
        NS_ASSERTION(sSingleton, "not initialized");
        return sSingleton;
    }

    virtual IFrameEmbeddingProtocolChild* IFrameEmbeddingConstructor(const MagicWindowHandle& hwnd);
    virtual nsresult IFrameEmbeddingDestructor(IFrameEmbeddingProtocolChild*);

private:
    static ContentProcessChild* sSingleton;

    DISALLOW_EVIL_CONSTRUCTORS(ContentProcessChild);
};

} // namespace dom
} // namespace mozilla

#endif
