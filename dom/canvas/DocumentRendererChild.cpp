/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/DocumentRendererChild.h"

#include "base/basictypes.h"

#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPattern.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsCSSParser.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsColor.h"
#include "nsLayoutUtils.h"
#include "nsContentUtils.h"
#include "nsCSSValue.h"
#include "nsRuleNode.h"
#include "mozilla/gfx/Matrix.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::ipc;

DocumentRendererChild::DocumentRendererChild()
{}

DocumentRendererChild::~DocumentRendererChild()
{}

bool
DocumentRendererChild::RenderDocument(nsPIDOMWindowOuter* window,
                                      const nsRect& documentRect,
                                      const mozilla::gfx::Matrix& transform,
                                      const nsString& aBGColor,
                                      uint32_t renderFlags,
                                      bool flushLayout,
                                      const nsIntSize& renderSize,
                                      nsCString& data)
{
    if (flushLayout)
        nsContentUtils::FlushLayoutForTree(window);

    RefPtr<nsPresContext> presContext;
    if (window) {
        nsIDocShell* docshell = window->GetDocShell();
        if (docshell) {
            docshell->GetPresContext(getter_AddRefs(presContext));
        }
    }
    if (!presContext)
        return false;

    nsCSSParser parser;
    nsCSSValue bgColorValue;
    if (!parser.ParseColorString(aBGColor, nullptr, 0, bgColorValue)) {
        return false;
    }

    nscolor bgColor;
    if (!nsRuleNode::ComputeColor(bgColorValue, presContext, nullptr, bgColor)) {
        return false;
    }

    // Draw directly into the output array.
    data.SetLength(renderSize.width * renderSize.height * 4);

    RefPtr<DrawTarget> dt =
        Factory::CreateDrawTargetForData(gfxPlatform::GetPlatform()->GetSoftwareBackend(),
                                         reinterpret_cast<uint8_t*>(data.BeginWriting()),
                                         IntSize(renderSize.width, renderSize.height),
                                         4 * renderSize.width,
                                         SurfaceFormat::B8G8R8A8);
    if (!dt || !dt->IsValid()) {
        gfxWarning() << "DocumentRendererChild::RenderDocument failed to Factory::CreateDrawTargetForData";
        return false;
    }
    RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(dt);
    MOZ_ASSERT(ctx); // already checked the draw target above
    ctx->SetMatrix(mozilla::gfx::ThebesMatrix(transform));

    nsCOMPtr<nsIPresShell> shell = presContext->PresShell();
    shell->RenderDocument(documentRect, renderFlags, bgColor, ctx);

    return true;
}
