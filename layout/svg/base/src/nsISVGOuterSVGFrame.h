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
#include "nsRect.h"

class nsISVGRenderer;
class nsIFrame;
class nsPresContext;

#define NS_ISVGOUTERSVGFRAME_IID \
{ 0x100666c3, 0x3c50, 0x4e3b, { 0xa4, 0x74, 0xe5, 0x4a, 0x18, 0x41, 0xe0, 0x3b } }

class nsISVGOuterSVGFrame : public nsISVGSVGFrame {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISVGOUTERSVGFRAME_IID)

  /* Invalidate takes a nsRect in screen pixel coordinates */
  NS_IMETHOD InvalidateRect(nsRect aRect) = 0;
  NS_IMETHOD IsRedrawSuspended(PRBool* isSuspended)=0;
  NS_IMETHOD SuspendRedraw()=0;
  NS_IMETHOD UnsuspendRedraw()=0;
  NS_IMETHOD GetRenderer(nsISVGRenderer**renderer)=0;
  NS_IMETHOD NotifyViewportChange()=0; // called by our correspoding content element
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISVGOuterSVGFrame, NS_ISVGOUTERSVGFRAME_IID)

#endif // __NS_ISVGOUTERSVGFRAME_H__
