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

#ifndef __NS_ISVGTEXTCONTENTMETRICS_H__
#define __NS_ISVGTEXTCONTENTMETRICS_H__

#include "nsISupports.h"
class nsIDOMSVGRect;
class nsIDOMSVGPoint;

////////////////////////////////////////////////////////////////////////
// nsISVGTextContentMetrics

// {CBF0A774-4171-4112-BD9A-F49BEFC0CE18}
#define NS_ISVGTEXTCONTENTMETRICS_IID \
{ 0xcbf0a774, 0x4171, 0x4112, { 0xbd, 0x9a, 0xf4, 0x9b, 0xef, 0xc0, 0xce, 0x18 } }

class nsISVGTextContentMetrics : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISVGTEXTCONTENTMETRICS_IID; return iid; }
  
  NS_IMETHOD GetNumberOfChars(PRInt32 *_retval)=0;
  NS_IMETHOD GetComputedTextLength(float *_retval)=0;
  NS_IMETHOD GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval)=0;
  NS_IMETHOD GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)=0;
  NS_IMETHOD GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)=0;
  NS_IMETHOD GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)=0;
  NS_IMETHOD GetRotationOfChar(PRUint32 charnum, float *_retval)=0;
  NS_IMETHOD GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval)=0;
};

#endif // __NS_ISVGTEXTCONTENTMETRICS_H__
