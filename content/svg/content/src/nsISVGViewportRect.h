/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef __NS_ISVGVIEWPORTRECT_H__
#define __NS_ISVGVIEWPORTRECT_H__

#include "nsIDOMSVGRect.h"

class nsISVGViewportAxis;

////////////////////////////////////////////////////////////////////////
// nsISVGViewportRect: private viewport interface

// {96547374-899B-434C-9566-0E23C58712F6}
#define NS_ISVGVIEWPORTRECT_IID \
{ 0x96547374, 0x899b, 0x434c, { 0x95, 0x66, 0x0e, 0x23, 0xc5, 0x87, 0x12, 0xf6 } }

class nsISVGViewportRect : public nsIDOMSVGRect
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISVGVIEWPORTRECT_IID; return iid; }

  NS_IMETHOD GetXAxis(nsISVGViewportAxis **xaxis)=0;
  NS_IMETHOD GetYAxis(nsISVGViewportAxis **yaxis)=0;
  NS_IMETHOD GetUnspecifiedAxis(nsISVGViewportAxis **axis)=0;
};


//////////////////////////////////////////////////////////////////////

class nsIDOMSVGNumber;

extern nsresult
NS_NewSVGViewportRect(nsISVGViewportRect **result,
                      nsIDOMSVGNumber* scalex,
                      nsIDOMSVGNumber* scaley,
                      nsIDOMSVGNumber* lengthx,
                      nsIDOMSVGNumber* lengthy);

#endif // __NS_ISVGVIEWPORTRECT_H__
