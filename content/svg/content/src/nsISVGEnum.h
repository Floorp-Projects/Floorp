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
 * IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Rowley <tor@cs.brown.edu> (original author)
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

#ifndef __NS_ISVGENUM_H__
#define __NS_ISVGENUM_H__

#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////
// nsISVGEnum: private interface for svg lengths

// {6bb710c5-a18c-45fc-a412-f042bae4da2d}
#define NS_ISVGENUM_IID \
{ 0x6bb710c5, 0xa18c, 0x45fc, { 0xa4, 0x12, 0xf0, 0x42, 0xba, 0xe4, 0xda, 0x2d } }

class nsISVGEnum : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGENUM_IID)

  NS_IMETHOD GetIntegerValue(PRUint16 &value)=0;
  NS_IMETHOD SetIntegerValue(PRUint16 value)=0;
};

#endif // __NS_ISVGENUM_H__

