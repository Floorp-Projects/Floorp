/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#ifndef mozilla_dom_ContentProcessChild_h
#define mozilla_dom_ContentProcessChild_h

#include "mozilla/dom/ContentProcessProtocolChild.h"

#include "nsTArray.h"
#include "nsAutoPtr.h"

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

    virtual TestShellProtocolChild* TestShellConstructor();
    virtual nsresult TestShellDestructor(TestShellProtocolChild*);

    void Quit();
    virtual nsresult RecvQuit();

private:
    static ContentProcessChild* sSingleton;

    nsTArray<nsAutoPtr<IFrameEmbeddingProtocolChild> > mIFrames;
    nsTArray<nsAutoPtr<TestShellProtocolChild> > mTestShells;

    PRBool mQuit;

    DISALLOW_EVIL_CONSTRUCTORS(ContentProcessChild);
};

} // namespace dom
} // namespace mozilla

#endif
