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

#ifndef __NS_ISVGTEXTCONTAINERFRAME_H__
#define __NS_ISVGTEXTCONTAINERFRAME_H__

#include "nsISupports.h"
#include "nsIDOMSVGLengthList.h"
class nsISVGTextFrame;

#define NS_ISVGTEXTCONTAINERFRAME_IID \
{ 0x3d3a12c4, 0x06bb, 0x4662, { 0xa3, 0xe3, 0x55, 0xaf, 0x5a, 0x48, 0x22, 0x78 } }

class nsISVGTextContainerFrame : public nsISupports
{
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGTEXTCONTAINERFRAME_IID)

  NS_IMETHOD_(nsISVGTextFrame *) GetTextFrame()=0;
  NS_IMETHOD_(PRBool) GetAbsolutePositionAdjustmentX(float &x, PRUint32 charNum)=0;
  NS_IMETHOD_(PRBool) GetAbsolutePositionAdjustmentY(float &y, PRUint32 charNum)=0;
  NS_IMETHOD_(PRBool) GetRelativePositionAdjustmentX(float &dx, PRUint32 charNum)=0;
  NS_IMETHOD_(PRBool) GetRelativePositionAdjustmentY(float &dy, PRUint32 charNum)=0;

  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetX()=0;
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetY()=0;
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDx()=0;
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDy()=0;
};

#endif // __NS_ISVGTEXTCONTAINERFRAME_H__
