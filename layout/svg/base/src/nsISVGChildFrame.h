/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __NS_ISVGCHILDFRAME_H__
#define __NS_ISVGCHILDFRAME_H__


#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsISVGRendererRegion.h"

class nsISVGRendererCanvas;
class nsPresContext;
class nsIDOMSVGRect;
struct nsRect;

// {1F23A200-B583-40EC-8564-9E6740C4DD3A}
#define NS_ISVGCHILDFRAME_IID \
{ 0x1f23a200, 0xb583, 0x40ec, { 0x85, 0x64, 0x9e, 0x67, 0x40, 0xc4, 0xdd, 0x3a } }

class nsISVGChildFrame : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGCHILDFRAME_IID)

  // XXX Ideally we don't want to pass the dirtyRect along but extract
  // it from nsIRenderingContext where needed (only in foreign objects)
  NS_IMETHOD Paint(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips)=0;
  NS_IMETHOD GetFrameForPoint(float x, float y, nsIFrame** hit)=0;
  NS_IMETHOD_(already_AddRefed<nsISVGRendererRegion>) GetCoveredRegion()=0;
  NS_IMETHOD InitialUpdate()=0;
  NS_IMETHOD NotifyCTMChanged()=0;
  NS_IMETHOD NotifyRedrawSuspended()=0;
  NS_IMETHOD NotifyRedrawUnsuspended()=0;

  // XXX move this function into interface nsISVGLocatableMetrics
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval)=0; // bbox in local coords
};

#endif // __NS_ISVGCHILDFRAME_H__

