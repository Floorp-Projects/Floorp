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
    virtual bool Recv__delete__(const PRInt32& x, const PRInt32& y,
                                const PRInt32& w, const PRInt32& h,
                                const PRUint32& nativeID);

private:
    nsCOMPtr<nsICanvasRenderingContextInternal> mCanvas;

    DISALLOW_EVIL_CONSTRUCTORS(DocumentRendererNativeIDParent);
};

}
}

#endif
