/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#ifndef mozilla_dom_ContentProcessParent_h
#define mozilla_dom_ContentProcessParent_h

#include "mozilla/dom/ContentProcessProtocolParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace dom {

class TabParent;

class ContentProcessParent
    : private ContentProcessProtocolParent
{
private:
    typedef mozilla::ipc::GeckoChildProcessHost GeckoChildProcessHost;

public:
    static ContentProcessParent* GetSingleton();

#if 0
    // TODO: implement this somewhere!
    static ContentProcessParent* FreeSingleton();
#endif

    TabParent* CreateTab(const MagicWindowHandle& hwnd);

private:
    static ContentProcessParent* gSingleton;

    // hide SendIFrameEmbeddingConstructor since we don't want client code
    // using it
    using ContentProcessProtocolParent::SendIFrameEmbeddingConstructor;

    ContentProcessParent();
    virtual ~ContentProcessParent();

    virtual IFrameEmbeddingProtocolParent* IFrameEmbeddingConstructor(const MagicWindowHandle& parentWidget);
    virtual nsresult IFrameEmbeddingDestructor(IFrameEmbeddingProtocolParent* frame);

    GeckoChildProcessHost mSubprocess;
};

} // namespace dom
} // namespace mozilla

#endif
