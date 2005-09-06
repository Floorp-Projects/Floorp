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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __NS_ISVGFILTER_H__
#define __NS_ISVGFILTER_H__

#define NS_ISVGFILTER_IID \
{ 0xbc1bf81a, 0x9765, 0x43c0, { 0xb3, 0x17, 0xd6, 0x91, 0x66, 0xcd, 0x3a, 0x30 } }

// Bitfields used by nsISVGFilter::GetRequirements()
#define NS_FE_SOURCEGRAPHIC   0x01
#define NS_FE_SOURCEALPHA     0x02
#define NS_FE_BACKGROUNDIMAGE 0x04
#define NS_FE_BACKGROUNDALPHA 0x08
#define NS_FE_FILLPAINT       0x10
#define NS_FE_STROKEPAINT     0x20

class nsSVGFilterInstance;

class nsISVGFilter : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGFILTER_IID)

  // Perform the filter operation on the images/regions
  // specified by 'instance'
  NS_IMETHOD Filter(nsSVGFilterInstance *instance) = 0;

  // Get the standard image source requirements of this filter.
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements) = 0;
};

#endif
