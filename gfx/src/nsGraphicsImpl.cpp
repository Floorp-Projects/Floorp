/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGraphicsImpl.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(nsGraphicsImpl, nsIGraphics)

nsGraphicsImpl::nsGraphicsImpl(nsIRenderingContext* aRenderer)
	:	mRenderer(aRenderer)
{
	NS_INIT_ISUPPORTS();
	
	// hack:  back out the coordinate transformation to use pixels.
	nsCOMPtr<nsIDeviceContext> dc;
	mRenderer->GetDeviceContext(*getter_AddRefs(dc));
	dc->GetDevUnitsToAppUnits(mDev2App);
	mRenderer->Scale(mDev2App, mDev2App);
}

NS_IMETHODIMP nsGraphicsImpl::GetColor(nscolor *aColor)
{
	return mRenderer->GetColor(*aColor);
}

NS_IMETHODIMP nsGraphicsImpl::SetColor(nscolor aColor)
{
	return mRenderer->SetColor(aColor);
}

NS_IMETHODIMP nsGraphicsImpl::ClipRect(nscoord x, nscoord y, nscoord width, nscoord height)
{
	nsRect r(x, y, width, height);
	PRBool clipEmpty;
	return mRenderer->SetClipRect(r, nsClipCombine_kIntersect, clipEmpty);
}

NS_IMETHODIMP nsGraphicsImpl::DrawLine(nscoord x1, nscoord y1, nscoord x2, nscoord y2)
{
	return mRenderer->DrawLine(x1, y1, x2, y2);
}

NS_IMETHODIMP nsGraphicsImpl::DrawRect(nscoord x, nscoord y, nscoord width, nscoord height)
{
	return mRenderer->DrawRect(x, y, width, height);
}

NS_IMETHODIMP nsGraphicsImpl::FillRect(nscoord x, nscoord y, nscoord width, nscoord height)
{
	return mRenderer->FillRect(x, y, width, height);
}

NS_IMETHODIMP nsGraphicsImpl::InvertRect(nscoord x, nscoord y, nscoord width, nscoord height)
{
	return mRenderer->InvertRect(x, y, width, height);
}

NS_IMETHODIMP nsGraphicsImpl::DrawEllipse(nscoord x, nscoord y, nscoord width, nscoord height)
{
	return mRenderer->DrawEllipse(x, y, width, height);
}

NS_IMETHODIMP nsGraphicsImpl::FillEllipse(nscoord x, nscoord y, nscoord width, nscoord height)
{
	return mRenderer->FillEllipse(x, y, width, height);
}

NS_IMETHODIMP nsGraphicsImpl::InvertEllipse(nscoord x, nscoord y, nscoord width, nscoord height)
{
	// return mRenderer->InvertEllipse(x, y, width, height);
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGraphicsImpl::DrawArc(nscoord x, nscoord y, nscoord width, nscoord height, float startAngle, float endAngle)
{
	return mRenderer->DrawArc(x, y, width, height, startAngle, endAngle);
}

NS_IMETHODIMP nsGraphicsImpl::FillArc(nscoord x, nscoord y, nscoord width, nscoord height, float startAngle, float endAngle)
{
	return mRenderer->FillArc(x, y, width, height, startAngle, endAngle);
}

NS_IMETHODIMP nsGraphicsImpl::InvertArc(nscoord x, nscoord y, nscoord width, nscoord height, float startAngle, float endAngle)
{
	// return mRenderer->InvertArc(x, y, width, height, startAngle, endAngle);
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGraphicsImpl::DrawPolygon(PRUint32 count, PRInt32 *points)
{
	return mRenderer->DrawPolygon((nsPoint*)points, count / 2);
}

NS_IMETHODIMP nsGraphicsImpl::FillPolygon(PRUint32 count, PRInt32 *points)
{
	return mRenderer->FillPolygon((nsPoint*)points, count / 2);
}

NS_IMETHODIMP nsGraphicsImpl::InvertPolygon(PRUint32 count, PRInt32 *points)
{
	// return mRenderer->InvertPolygon(points);
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGraphicsImpl::DrawString(const PRUnichar *text, nscoord x, nscoord y)
{
  return mRenderer->DrawString(text, nsCRT::strlen(text), x, y);
}

NS_IMETHODIMP nsGraphicsImpl::SetFont(const PRUnichar *name, nscoord size)
{
	size = NS_STATIC_CAST(nscoord, size * mDev2App);
	nsAutoString nameString(name);
	nsFont font(nameString, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE, size);
	return mRenderer->SetFont(font);
}

NS_IMETHODIMP nsGraphicsImpl::Gsave(void)
{
	return mRenderer->PushState();
}

NS_IMETHODIMP nsGraphicsImpl::Grestore(void)
{
	PRBool unused;
	return mRenderer->PopState(unused);
}
