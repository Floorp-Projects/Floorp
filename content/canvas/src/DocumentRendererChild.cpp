/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/DocumentRendererChild.h"

#include "base/basictypes.h"

#include "gfxImageSurface.h"
#include "gfxPattern.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsCSSParser.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsColor.h"
#include "gfxContext.h"
#include "nsLayoutUtils.h"
#include "nsContentUtils.h"

using namespace mozilla::ipc;

DocumentRendererChild::DocumentRendererChild()
{}

DocumentRendererChild::~DocumentRendererChild()
{}

bool
DocumentRendererChild::RenderDocument(nsIDOMWindow *window,
                                      const nsRect& documentRect,
                                      const gfxMatrix& transform,
                                      const nsString& bgcolor,
                                      PRUint32 renderFlags,
                                      bool flushLayout, 
                                      const nsIntSize& renderSize,
                                      nsCString& data)
{
    if (flushLayout)
        nsContentUtils::FlushLayoutForTree(window);

    nsCOMPtr<nsPresContext> presContext;
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(window);
    if (win) {
        nsIDocShell* docshell = win->GetDocShell();
        if (docshell) {
            docshell->GetPresContext(getter_AddRefs(presContext));
        }
    }
    if (!presContext)
        return false;

    nscolor bgColor;
    nsCSSParser parser;
    nsresult rv = parser.ParseColorString(bgcolor, nsnull, 0, &bgColor);
    if (NS_FAILED(rv))
        return false;

    nsIPresShell* presShell = presContext->PresShell();

    // Draw directly into the output array.
    data.SetLength(renderSize.width * renderSize.height * 4);

    nsRefPtr<gfxImageSurface> surf =
        new gfxImageSurface(reinterpret_cast<uint8_t*>(data.BeginWriting()),
                            gfxIntSize(renderSize.width, renderSize.height),
                            4 * renderSize.width,
                            gfxASurface::ImageFormatARGB32);
    nsRefPtr<gfxContext> ctx = new gfxContext(surf);
    ctx->SetMatrix(transform);

    presShell->RenderDocument(documentRect, renderFlags, bgColor, ctx);

    return true;
}
