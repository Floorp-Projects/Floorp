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

#ifndef __NS_ISVGOUTERSVGFRAME_H__
#define __NS_ISVGOUTERSVGFRAME_H__

#include "nsISVGSVGFrame.h"

class nsISVGRenderer;
class nsIFrame;
class nsISVGRendererRegion;
class nsPresContext;

// {5A889B3C-A235-41E2-8AB4-65547FCC79DB}
#define NS_ISVGOUTERSVGFRAME_IID \
{ 0x5a889b3c, 0xa235, 0x41e2, { 0x8a, 0xb4, 0x65, 0x54, 0x7f, 0xcc, 0x79, 0xdb } }

class nsISVGOuterSVGFrame : public nsISVGSVGFrame {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGOUTERSVGFRAME_IID)

  NS_IMETHOD InvalidateRegion(nsISVGRendererRegion *region, PRBool bRedraw)=0;
  NS_IMETHOD IsRedrawSuspended(PRBool* isSuspended)=0;
  NS_IMETHOD SuspendRedraw()=0;
  NS_IMETHOD UnsuspendRedraw()=0;
  NS_IMETHOD GetRenderer(nsISVGRenderer**renderer)=0;
  NS_IMETHOD CreateSVGRect(nsIDOMSVGRect **_retval)=0;
  NS_IMETHOD NotifyViewportChange()=0; // called by our correspoding content element
};

#endif // __NS_ISVGOUTERSVGFRAME_H__
