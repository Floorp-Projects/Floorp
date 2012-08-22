/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentRendererNativeIDParent
#define mozilla_dom_DocumentRendererNativeIDParent

#include "mozilla/ipc/PDocumentRendererNativeIDParent.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace ipc {

class DocumentRendererNativeIDParent : public PDocumentRendererNativeIDParent
{
public:
    DocumentRendererNativeIDParent();
    virtual ~DocumentRendererNativeIDParent();

    void SetCanvas(nsICanvasRenderingContextInternal* aCanvas);
    virtual bool Recv__delete__(const int32_t& x, const int32_t& y,
                                const int32_t& w, const int32_t& h,
                                const uint32_t& nativeID);

private:
    nsCOMPtr<nsICanvasRenderingContextInternal> mCanvas;

    DISALLOW_EVIL_CONSTRUCTORS(DocumentRendererNativeIDParent);
};

}
}

#endif
