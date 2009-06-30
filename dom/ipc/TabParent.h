/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#ifndef mozilla_tabs_TabParent_h
#define mozilla_tabs_TabParent_h

#include "TabTypes.h"
#include "IFrameEmbeddingProtocol.h"
#include "IFrameEmbeddingProtocolParent.h"
#include "TabProcessParent.h"

#include "mozilla/Monitor.h"

class nsIURI;

namespace mozilla {
namespace tabs {

class TabParent
    : private IFrameEmbeddingProtocol::Parent
{
public:
    TabParent(MagicWindowHandle parentWidget);
    virtual ~TabParent();

    void LoadURL(nsIURI* aURI);
    void Move(PRUint32 x, PRUint32 y, PRUint32 width, PRUint32 height);

private:
    void LaunchSubprocess();

    TabProcessParent mSubprocess;
    IFrameEmbeddingProtocolParent mParent;

    mozilla::Monitor mMonitor;
};

} // namespace tabs
} // namespace mozilla

#endif
