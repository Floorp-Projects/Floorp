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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "nsICanvasBoxObject.h"
#include "nsCanvasFrame.h"

#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIBoxObject.h"
#include "nsIFrame.h"
#include "nsBoxObject.h"

class nsCanvasBoxObject : public nsICanvasBoxObject, public nsBoxObject
{
public:
    nsCanvasBoxObject();
    virtual ~nsCanvasBoxObject();

    nsICanvasBoxObject* GetFrameBoxObject();

    // nsISupports interface
    NS_DECL_ISUPPORTS_INHERITED

    // nsICanvasBoxObject interface
    NS_DECL_NSICANVASBOXOBJECT

    // nsPIBoxObject interface
    NS_IMETHOD Init(nsIContent* aContent, nsIPresShell* aPresShell);
    NS_IMETHOD SetDocument(nsIDocument* aDocument);
    NS_IMETHOD InvalidatePresentationStuff();
};

NS_INTERFACE_MAP_BEGIN(nsCanvasBoxObject)
    NS_INTERFACE_MAP_ENTRY(nsICanvasBoxObject)
NS_INTERFACE_MAP_END_INHERITING(nsBoxObject)

NS_IMPL_ADDREF(nsCanvasBoxObject)
NS_IMPL_RELEASE(nsCanvasBoxObject)

nsCanvasBoxObject::nsCanvasBoxObject()
{
    NS_INIT_ISUPPORTS();
}

nsCanvasBoxObject::~nsCanvasBoxObject()
{
}

inline nsICanvasBoxObject *
nsCanvasBoxObject::GetFrameBoxObject()
{
    nsIFrame *frame = GetFrame();
    nsICanvasBoxObject *frameCanvasBoxObject = nsnull;
    CallQueryInterface(frame, &frameCanvasBoxObject);
    return frameCanvasBoxObject;
}

// nsPIBoxObject
NS_IMETHODIMP
nsCanvasBoxObject::Init(nsIContent* aContent, nsIPresShell* aPresShell)
{
    return nsBoxObject::Init(aContent, aPresShell);
}

NS_IMETHODIMP
nsCanvasBoxObject::SetDocument(nsIDocument* aDocument)
{
    return nsBoxObject::SetDocument(aDocument);
}

NS_IMETHODIMP
nsCanvasBoxObject::InvalidatePresentationStuff()
{
    return nsBoxObject::InvalidatePresentationStuff();
}

// nsICanvasBoxObject
NS_IMETHODIMP
nsCanvasBoxObject::GetContext(const char *aContext, nsISupports **aResult)
{
    nsICanvasBoxObject *canvas = GetFrameBoxObject();
    if (canvas)
        return GetFrameBoxObject()->GetContext(aContext, aResult);
    return NS_ERROR_FAILURE;
}

// creation
nsresult
NS_NewCanvasBoxObject(nsIBoxObject **aResult)
{
    *aResult = new nsCanvasBoxObject;
    if (!*aResult)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aResult);
    return NS_OK;
}
