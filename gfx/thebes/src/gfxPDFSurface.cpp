/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Foundation Code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#include "gfxPDFSurface.h"

#include "cairo.h"
#include "cairo-pdf.h"

static cairo_status_t
write_func(void *closure, const unsigned char *data, unsigned int length)
{
    nsCOMPtr<nsIOutputStream> out = reinterpret_cast<nsIOutputStream*>(closure);
    do {
        PRUint32 wrote = 0;
        if (NS_FAILED(out->Write((const char*)data, length, &wrote)))
            break;
        data += wrote; length -= wrote;
    } while (length > 0);
    NS_ASSERTION(length == 0, "not everything was written to the file");
    return CAIRO_STATUS_SUCCESS;
}

gfxPDFSurface::gfxPDFSurface(nsIOutputStream *aStream, const gfxSize& aSizeInPoints)
    : mStream(aStream), mXDPI(-1), mYDPI(-1), mSize(aSizeInPoints)
{
    Init(cairo_pdf_surface_create_for_stream(write_func, (void*)mStream, mSize.width, mSize.height));
}

gfxPDFSurface::~gfxPDFSurface()
{
}

nsresult
gfxPDFSurface::BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName)
{
    return NS_OK;
}

nsresult
gfxPDFSurface::EndPrinting()
{
    return NS_OK;
}

nsresult
gfxPDFSurface::AbortPrinting()
{
    return NS_OK;
}

nsresult
gfxPDFSurface::BeginPage()
{
    return NS_OK;
}

nsresult
gfxPDFSurface::EndPage()
{
    cairo_t *cx = cairo_create(CairoSurface());
    cairo_show_page(cx);
    cairo_destroy(cx);
    return NS_OK;
}

void
gfxPDFSurface::Finish()
{
    gfxASurface::Finish();
    mStream->Close();
}

void
gfxPDFSurface::SetDPI(double xDPI, double yDPI)
{
    mXDPI = xDPI;
    mYDPI = yDPI;
    cairo_surface_set_fallback_resolution(CairoSurface(), xDPI, yDPI);
}

void
gfxPDFSurface::GetDPI(double *xDPI, double *yDPI)
{
    *xDPI = mXDPI;
    *yDPI = mYDPI;
}
