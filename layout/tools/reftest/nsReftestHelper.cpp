/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is layout reftest.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include <string.h>

#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsISupports.h"

#include "nsIDOMHTMLCanvasElement.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"

#include "nsIReftestHelper.h"

#include "gfxContext.h"
#include "gfxImageSurface.h"

#include "nsStringGlue.h"

class nsReftestHelper :
    public nsIReftestHelper
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREFTESTHELPER
};

NS_IMPL_ISUPPORTS1(nsReftestHelper, nsIReftestHelper)

static already_AddRefed<gfxImageSurface>
CanvasToImageSurface(nsIDOMHTMLCanvasElement *canvas)
{
    PRUint32 w, h;
    nsresult rv;

    nsCOMPtr<nsICanvasElement> elt = do_QueryInterface(canvas);
    rv = elt->GetSize(&w, &h);
    if (NS_FAILED(rv))
        return nsnull;

    nsCOMPtr<nsISupports> ctxg;
    rv = canvas->GetContext(NS_LITERAL_STRING("2d"), getter_AddRefs(ctxg));
    if (NS_FAILED(rv))
        return nsnull;

    nsCOMPtr<nsICanvasRenderingContextInternal> ctx = do_QueryInterface(ctxg);
    if (ctx == nsnull)
        return nsnull;

    nsRefPtr<gfxImageSurface> img = new gfxImageSurface(gfxIntSize(w, h), gfxASurface::ImageFormatARGB32);
    if (img == nsnull)
        return nsnull;

    nsRefPtr<gfxContext> t = new gfxContext(img);
    if (t == nsnull)
        return nsnull;

    t->SetOperator(gfxContext::OPERATOR_CLEAR);
    t->Paint();

    t->SetOperator(gfxContext::OPERATOR_OVER);
    rv = ctx->Render(t);
    if (NS_FAILED(rv))
        return nsnull;

    NS_ADDREF(img.get());
    return img.get();
}

NS_IMETHODIMP
nsReftestHelper::CompareCanvas(nsIDOMHTMLCanvasElement *canvas1,
                               nsIDOMHTMLCanvasElement *canvas2,
                               PRUint32 *result)
{
    if (canvas1 == nsnull ||
        canvas2 == nsnull ||
        result == nsnull)
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxImageSurface> img1 = CanvasToImageSurface(canvas1);
    nsRefPtr<gfxImageSurface> img2 = CanvasToImageSurface(canvas2);

    if (img1 == nsnull || img2 == nsnull ||
        img1->GetSize() != img2->GetSize() ||
        img1->Stride() != img2->Stride())
        return NS_ERROR_FAILURE;

    int v;
    gfxIntSize size = img1->GetSize();
    PRUint32 stride = img1->Stride();

    // we can optimize for the common all-pass case
    if (stride == size.width * 4) {
        v = memcmp(img1->Data(), img2->Data(), size.width * size.height * 4);
        if (v == 0) {
            *result = 0;
            return NS_OK;
        }
    }

    PRUint32 different = 0;
    for (int j = 0; j < size.height; j++) {
        unsigned char *p1 = img1->Data() + j*stride;
        unsigned char *p2 = img2->Data() + j*stride;
        v = memcmp(p1, p2, stride);

        if (v) {
            for (int i = 0; i < size.width; i++) {
                if (*(PRUint32*) p1 != *(PRUint32*) p2)
                    different++;
                p1 += 4;
                p2 += 4;
            }
        }
    }

    *result = different;
    return NS_OK;
}


NS_GENERIC_FACTORY_CONSTRUCTOR(nsReftestHelper)

static const nsModuleComponentInfo components[] =
{
    { "nsReftestHelper",
      { 0x0269F9AD, 0xA2BD, 0x4AEA,
        { 0xB9, 0x2F, 0xCB, 0xF2, 0x23, 0x80, 0x94, 0x54 } },
      "@mozilla.org/reftest-helper;1",
      nsReftestHelperConstructor },
};

NS_IMPL_NSGETMODULE(nsReftestModule, components)

