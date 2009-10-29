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

#include "mozilla/ipc/DocumentRendererChild.h"
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
#include "nsICSSParser.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsColor.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsLayoutUtils.h"

using namespace mozilla::ipc;

DocumentRendererChild::DocumentRendererChild()
{}

DocumentRendererChild::~DocumentRendererChild()
{}

static void
FlushLayoutForTree(nsIDOMWindow* aWindow)
{
    nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aWindow);
    if (!piWin)
        return;

    // Note that because FlushPendingNotifications flushes parents, this
    // is O(N^2) in docshell tree depth.  However, the docshell tree is
    // usually pretty shallow.

    nsCOMPtr<nsIDOMDocument> domDoc;
    aWindow->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    if (doc) {
        doc->FlushPendingNotifications(Flush_Layout);
    }

    nsCOMPtr<nsIDocShellTreeNode> node =
        do_QueryInterface(piWin->GetDocShell());
    if (node) {
        PRInt32 i = 0, i_end;
        node->GetChildCount(&i_end);
        for (; i < i_end; ++i) {
            nsCOMPtr<nsIDocShellTreeItem> item;
            node->GetChildAt(i, getter_AddRefs(item));
            nsCOMPtr<nsIDOMWindow> win = do_GetInterface(item);
            if (win) {
                FlushLayoutForTree(win);
            }
        }
    }
}

bool
DocumentRendererChild::RenderDocument(nsIDOMWindow *window, const PRInt32& x, const PRInt32& y, const PRInt32& w, const PRInt32& h,
                                      const nsString& aBGColor, const PRUint32& flags, const PRBool& flush, 
                                      PRUint32& _width, PRUint32& _height, nsCString& data)
{
    if (flush)
        FlushLayoutForTree(window);

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
    nsCOMPtr<nsICSSParser> parser = do_CreateInstance("@mozilla.org/content/css-parser;1");
    nsresult rv = parser->ParseColorString(PromiseFlatString(aBGColor),
                                           nsnull, 0, &bgColor);
    if (NS_FAILED(rv))
        return false;

    nsIPresShell* presShell = presContext->PresShell();

    nsRect r(x, y, w, h);

    _width = nsPresContext::AppUnitsToIntCSSPixels(w);
    _height = nsPresContext::AppUnitsToIntCSSPixels(h);

    // Draw directly into the output array.
    data.SetLength(_width * _height * 4);
    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(reinterpret_cast<PRUint8*>(const_cast<char*>(data.get())),
                                                         gfxIntSize(_width, _height),
                                                         4 * _width, gfxASurface::ImageFormatARGB32);
    nsRefPtr<gfxContext> ctx = new gfxContext(surf);

    PRBool oldDisableValue = nsLayoutUtils::sDisableGetUsedXAssertions;
    nsLayoutUtils::sDisableGetUsedXAssertions = oldDisableValue || !flush;
    presShell->RenderDocument(r, flags, bgColor, ctx);
    nsLayoutUtils::sDisableGetUsedXAssertions = oldDisableValue;

    return true;
}
