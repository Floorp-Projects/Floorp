/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/DocumentRendererParent.h"

#include "gfx2DGlue.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/RefPtr.h"
#include "nsICanvasRenderingContextInternal.h"

using namespace mozilla;
using namespace mozilla::gfx;
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

    DrawTarget* drawTarget = mCanvasContext->GetDrawTarget();
    Rect rect(0, 0, aSize.width, aSize.height);
    MaybeSnapToDevicePixels(rect, *drawTarget, true);
    RefPtr<DataSourceSurface> dataSurface =
        Factory::CreateWrappingDataSourceSurface(reinterpret_cast<uint8_t*>(const_cast<nsCString&>(aData).BeginWriting()),
                                                 aSize.width * 4,
                                                 IntSize(aSize.width, aSize.height),
                                                 SurfaceFormat::B8G8R8A8);
    SurfacePattern pattern(dataSurface, ExtendMode::CLAMP);
    drawTarget->FillRect(rect, pattern);

    gfxRect damageRect = mCanvasContext->UserToDevice(ThebesRect(rect));
    mCanvas->Redraw(damageRect);
}

void
DocumentRendererParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005139
}

bool
DocumentRendererParent::Recv__delete__(const nsIntSize& renderedSize,
                                       const nsCString& data)
{
    DrawToCanvas(renderedSize, data);
    return true;
}
