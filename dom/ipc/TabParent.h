/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#ifndef mozilla_tabs_TabParent_h
#define mozilla_tabs_TabParent_h

#include "mozilla/dom/IFrameEmbeddingProtocolParent.h"

#include "mozilla/ipc/GeckoChildProcessHost.h"

class nsIURI;

namespace mozilla {
namespace dom {

class TabParent
    : public IFrameEmbeddingProtocolParent
{
public:
    TabParent();
    virtual ~TabParent();

    void LoadURL(nsIURI* aURI);
    void Move(PRUint32 x, PRUint32 y, PRUint32 width, PRUint32 height);
};

} // namespace dom
} // namespace mozilla

#endif
