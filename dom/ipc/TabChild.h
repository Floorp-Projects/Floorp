/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */

#ifndef mozilla_tabs_TabChild_h
#define mozilla_tabs_TabChild_h

#include "mozilla/dom/IFrameEmbeddingProtocolChild.h"
#include "nsIWebNavigation.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class TabChild
    : public IFrameEmbeddingProtocolChild
{
public:
    TabChild(const MagicWindowHandle& parentWidget);
    virtual ~TabChild();

    virtual nsresult RecvloadURL(const nsCString& uri);
    virtual nsresult Recvmove(const PRUint32& x,
                              const PRUint32& y,
                              const PRUint32& width,
                              const PRUint32& height);

private:
    nsCOMPtr<nsIWebNavigation> mWebNav;

    DISALLOW_EVIL_CONSTRUCTORS(TabChild);
};

}
}

#endif // mozilla_tabs_TabChild_h
