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

#ifndef __NS_ISVGVIEWPORTAXIS_H__
#define __NS_ISVGVIEWPORTAXIS_H__

#include "nsISupports.h"

class nsIDOMSVGNumber;

////////////////////////////////////////////////////////////////////////
// nsISVGViewportAxis: private interface used as a context for
// percentage/unit-based calculations on svg lengths

// {62C247BD-0CF0-43B6-A937-6BD7875B5E0E}
#define NS_ISVGVIEWPORTAXIS_IID \
{ 0x62c247bd, 0x0cf0, 0x43b6, { 0xa9, 0x37, 0x6b, 0xd7, 0x87, 0x5b, 0x5e, 0x0e } }

class nsISVGViewportAxis : public nsISupports
{
  public:
    static const nsIID& GetIID() { static nsIID iid = NS_ISVGVIEWPORTAXIS_IID; return iid; }

    NS_IMETHOD GetMillimeterPerPixel(nsIDOMSVGNumber **scale)=0;
    NS_IMETHOD GetLength(nsIDOMSVGNumber **length)=0;
};

#endif // __NS_ISVGVIEWPORTAXIS_H__


