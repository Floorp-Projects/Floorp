/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Fennec Electrolysis.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "gfxImageSurface.h"
#include "nsLayoutUtils.h"

#include "mozilla/ipc/DocumentRendererChild.h"

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
                                      PRBool flushLayout, 
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
    nsresult rv = parser.ParseColorString(PromiseFlatString(bgcolor),
                                          nsnull, 0, &bgColor);
    if (NS_FAILED(rv))
        return false;

    nsIPresShell* presShell = presContext->PresShell();

    // Draw directly into the output array.
    data.SetLength(renderSize.width * renderSize.height * 4);

    nsRefPtr<gfxImageSurface> surf =
        new gfxImageSurface(reinterpret_cast<uint8*>(data.BeginWriting()),
                            gfxIntSize(renderSize.width, renderSize.height),
                            4 * renderSize.width,
                            gfxASurface::ImageFormatARGB32);
    nsRefPtr<gfxContext> ctx = new gfxContext(surf);
    ctx->SetMatrix(transform);

    presShell->RenderDocument(documentRect, renderFlags, bgColor, ctx);

    return true;
}
