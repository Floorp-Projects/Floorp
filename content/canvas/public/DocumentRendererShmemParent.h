/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentRendererShmemParent
#define mozilla_dom_DocumentRendererShmemParent

#include "mozilla/ipc/PDocumentRendererShmemParent.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace ipc {

class DocumentRendererShmemParent : public PDocumentRendererShmemParent
{
public:
    DocumentRendererShmemParent();
    virtual ~DocumentRendererShmemParent();

    void SetCanvas(nsICanvasRenderingContextInternal* aCanvas);
    virtual bool Recv__delete__(const int32_t& x, const int32_t& y,
                                const int32_t& w, const int32_t& h,
                                Shmem& data);

private:
    nsCOMPtr<nsICanvasRenderingContextInternal> mCanvas;

    DISALLOW_EVIL_CONSTRUCTORS(DocumentRendererShmemParent);
};

}
}

#endif
