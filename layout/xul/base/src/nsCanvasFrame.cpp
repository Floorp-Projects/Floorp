/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsCanvasFrame.h"
#include "nsICanvasRenderingContext.h"
#include "nsIComponentManager.h"

nsresult
NS_NewCanvasXULFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
    NS_ENSURE_ARG_POINTER(aPresShell);
    NS_ENSURE_ARG_POINTER(aNewFrame);

    nsCanvasFrame* frame = new (aPresShell) nsCanvasFrame(aPresShell);
    if (!frame)
        return NS_ERROR_OUT_OF_MEMORY;

    *aNewFrame = frame;
    return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsCanvasFrame)
    NS_INTERFACE_MAP_ENTRY(nsICanvasBoxObject)
NS_INTERFACE_MAP_END_INHERITING(nsLeafBoxFrame)

nsCanvasFrame::nsCanvasFrame(nsIPresShell* aPresShell)
    : nsLeafBoxFrame(aPresShell), mPresContext(nsnull)
{
}

nsCanvasFrame::~nsCanvasFrame()
{
}

NS_IMETHODIMP_(nsrefcnt)
nsCanvasFrame::AddRef(void)
{
    // don't do this!
    return 0;
}

NS_IMETHODIMP_(nsrefcnt)
nsCanvasFrame::Release(void)
{
    // don't do this!
    return 0;
}

//
// overriden nsIFrame stuff
//
NS_IMETHODIMP
nsCanvasFrame::Init(nsPresContext* aPresContext,
                    nsIContent* aContent,
                    nsIFrame* aParent,
                    nsStyleContext* aContext,
                    nsIFrame* aPrevInFlow)
{
    mPresContext = aPresContext;

    return nsLeafBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
}

NS_IMETHODIMP
nsCanvasFrame::Destroy(nsPresContext* aPresContext)
{
    return nsLeafBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsCanvasFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
    aSize.width = 0;
    aSize.height = 0;

    AddBorderAndPadding(aSize);
    AddInset(aSize);
    nsIBox::AddCSSPrefSize(aState, this, aSize);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasFrame::Paint(nsPresContext* aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect,
                     nsFramePaintLayer aWhichLayer,
                     PRUint32 aFlags)
{
//    fprintf (stderr, "+++ nsCanvasFrame::Paint\n");
    if (!mRenderingContext)
        return NS_OK;

    return mRenderingContext->Paint(aPresContext, aRenderingContext,
                                    aDirtyRect, aWhichLayer,
                                    aFlags);
}

NS_IMETHODIMP
nsCanvasFrame::GetContext(const char* aContext, nsISupports** aResult)
{
    NS_ENSURE_ARG(aContext);
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;

    nsCAutoString contractID("@mozilla.org/layout/canvas-rendering-context?name=");

    contractID += aContext;
    contractID += ";1";

    if (mRenderingContext) {
        if (! mRenderingContextName.Equals(aContext))
            return NS_ERROR_FAILURE;
    } else {
        mRenderingContext = do_CreateInstance(contractID.get());
        if (!mRenderingContext)
            return NS_ERROR_FAILURE;

        rv = mRenderingContext->Init(this, mPresContext);
        fprintf (stderr, "nsCanvasRenderingContextInternal::Init returned: 0x%08x\n", rv);
        if (NS_FAILED(rv)) {
            mRenderingContext = nsnull;
            return rv;
        }
        mRenderingContextName.Assign(aContext);
    }

    *aResult = mRenderingContext.get();
    NS_ADDREF(*aResult);

    return NS_OK;
}

