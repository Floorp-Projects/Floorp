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

#ifndef __NS_ISVGLENGTHLIST_H__
#define __NS_ISVGLENGTHLIST_H__

#include "nsIDOMSVGLengthList.h"

class nsISVGViewportAxis;

////////////////////////////////////////////////////////////////////////
// nsISVGLengthList: private interface for svg lengthlists

// {3B57DE3C-A2C3-4C17-B47A-0791C2EE0BE1}
#define NS_ISVGLENGTHLIST_IID \
{ 0x3b57de3c, 0xa2c3, 0x4c17, { 0xb4, 0x7a, 0x07, 0x91, 0xc2, 0xee, 0x0b, 0xe1 } }

class nsISVGLengthList : public nsIDOMSVGLengthList
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISVGLENGTHLIST_IID; return iid; }

  NS_IMETHOD SetContext(nsISVGViewportAxis* axis)=0;
};


#endif // __NS_ISVGLENGTHLIST_H__
