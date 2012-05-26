/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/DocumentRendererParent.h"
#include "gfxImageSurface.h"
#include "gfxPattern.h"

using namespace mozilla::ipc;

DocumentRendererParent::DocumentRendererParent()
{}

DocumentRendererParent::~DocumentRendererParent()
{}

void DocumentRendererParent::SetCanvasContext(nsICanvasRenderingContextInternal* aCanvas,
                                              gfxContext* ctx)
{
    mCanvas = aCanvas;
    mCanvasContext = ctx;
}

void DocumentRendererParent::DrawToCanvas(const nsIntSize& aSize,
                                          const nsCString& aData)
{
    if (!mCanvas || !mCanvasContext)
        return;

    nsRefPtr<gfxImageSurface> surf =
        new gfxImageSurface(reinterpret_cast<uint8_t*>(const_cast<nsCString&>(aData).BeginWriting()),
                            gfxIntSize(aSize.width, aSize.height),
                            aSize.width * 4,
                            gfxASurface::ImageFormatARGB32);
    nsRefPtr<gfxPattern> pat = new gfxPattern(surf);

    gfxRect rect(gfxPoint(0, 0), gfxSize(aSize.width, aSize.height));
    mCanvasContext->NewPath();
    mCanvasContext->PixelSnappedRectangleAndSetPattern(rect, pat);
    mCanvasContext->Fill();

    // get rid of the pattern surface ref, because aData is very
    // likely to go away shortly
    mCanvasContext->SetColor(gfxRGBA(1,1,1,1));

    gfxRect damageRect = mCanvasContext->UserToDevice(rect);
    mCanvas->Redraw(damageRect);
}

bool
DocumentRendererParent::Recv__delete__(const nsIntSize& renderedSize,
                                       const nsCString& data)
{
    DrawToCanvas(renderedSize, data);
    return true;
}
