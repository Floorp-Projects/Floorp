/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include <stdlib.h>
#include <stdarg.h>

#include "prmem.h"
#include "prprf.h"

#include "nsIServiceManager.h"

#include "nsIConsoleService.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsHTMLCanvasElement.h"
#include "nsIPrincipal.h"
#include "nsINode.h"

#include "nsGfxCIID.h"

#include "nsTArray.h"

#include "CanvasUtils.h"

using namespace mozilla;

void
CanvasUtils::DoDrawImageSecurityCheck(nsHTMLCanvasElement *aCanvasElement,
                                      nsIPrincipal *aPrincipal,
                                      PRBool forceWriteOnly)
{
    // Callers should ensure that mCanvasElement is non-null before calling this
    if (!aCanvasElement) {
        NS_WARNING("DoDrawImageSecurityCheck called without canvas element!");
        return;
    }

    if (aCanvasElement->IsWriteOnly())
        return;

    // If we explicitly set WriteOnly just do it and get out
    if (forceWriteOnly) {
        aCanvasElement->SetWriteOnly();
        return;
    }

    if (aPrincipal == nsnull)
        return;

    PRBool subsumes;
    nsresult rv =
        aCanvasElement->NodePrincipal()->Subsumes(aPrincipal, &subsumes);

    if (NS_SUCCEEDED(rv) && subsumes) {
        // This canvas has access to that image anyway
        return;
    }

    aCanvasElement->SetWriteOnly();
}

void
CanvasUtils::LogMessage (const nsCString& errorString)
{
    nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (!console)
        return;

    console->LogStringMessage(NS_ConvertUTF8toUTF16(errorString).get());
    fprintf(stderr, "%s\n", errorString.get());
}

void
CanvasUtils::LogMessagef (const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[256];

    nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (console) {
        PR_vsnprintf(buf, 256, fmt, ap);
        console->LogStringMessage(NS_ConvertUTF8toUTF16(nsDependentCString(buf)).get());
        fprintf(stderr, "%s\n", buf);
    }

    va_end(ap);
}
