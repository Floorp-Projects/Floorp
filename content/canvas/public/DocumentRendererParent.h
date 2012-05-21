/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentRendererParent
#define mozilla_dom_DocumentRendererParent

#include "mozilla/ipc/PDocumentRendererParent.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "gfxContext.h"

namespace mozilla {
namespace ipc {

class DocumentRendererParent : public PDocumentRendererParent
{
public:
    DocumentRendererParent();
    virtual ~DocumentRendererParent();

    void SetCanvasContext(nsICanvasRenderingContextInternal* aCanvas,
			  gfxContext* ctx);
    void DrawToCanvas(const nsIntSize& renderedSize,
		      const nsCString& aData);

    virtual bool Recv__delete__(const nsIntSize& renderedSize,
                                const nsCString& data);

private:
    nsCOMPtr<nsICanvasRenderingContextInternal> mCanvas;
    nsRefPtr<gfxContext> mCanvasContext;

    DISALLOW_EVIL_CONSTRUCTORS(DocumentRendererParent);
};

}
}

#endif
