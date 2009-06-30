/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */

#ifndef mozilla_tabs_TabChild_h
#define mozilla_tabs_TabChild_h

#include "TabTypes.h"
#include "IFrameEmbeddingProtocol.h"
#include "IFrameEmbeddingProtocolChild.h"
#include "nsIWebNavigation.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace tabs {

class TabChild
    : public IFrameEmbeddingProtocol::Child
{
private:
    typedef mozilla::ipc::String String;

public:
    TabChild();
    virtual ~TabChild();

    bool Init(MessageLoop* aIOLoop, IPC::Channel* aChannel);

    virtual nsresult init(const MagicWindowHandle& parentWidget);
    virtual nsresult loadURL(const String& uri);
    virtual nsresult move(const uint32_t& x,
                          const uint32_t& y,
                          const uint32_t& width,
                          const uint32_t& height);

private:
    MagicWindowHandle mWidget;
    IFrameEmbeddingProtocolChild mChild;
    nsCOMPtr<nsIWebNavigation> mWebNav;

    DISALLOW_EVIL_CONSTRUCTORS(TabChild);
};

}
}

#endif // mozilla_tabs_TabChild_h
