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


#ifndef __NS_ISVGGLYPHFRAGMENTNODE_H__
#define __NS_ISVGGLYPHFRAGMENTNODE_H__

#include "nsISupports.h"

class nsISVGGlyphFragmentLeaf;

// {5A3A3929-9F98-4756-8C85-9689EB05983B}
#define NS_ISVGGLYPHFRAGMENTNODE_IID \
{ 0x5a3a3929, 0x9f98, 0x4756, { 0x8c, 0x85, 0x96, 0x89, 0xeb, 0x05, 0x98, 0x3b } }

class nsISVGGlyphFragmentNode : public nsISupports
{
public:
  
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGGLYPHFRAGMENTNODE_IID)

  NS_IMETHOD_(nsISVGGlyphFragmentLeaf *) GetFirstGlyphFragment()=0;
  NS_IMETHOD_(nsISVGGlyphFragmentLeaf *) GetNextGlyphFragment()=0;
  NS_IMETHOD_(PRUint32) BuildGlyphFragmentTree(PRUint32 charNum, PRBool lastBranch)=0;
  NS_IMETHOD_(void) NotifyMetricsSuspended()=0;
  NS_IMETHOD_(void) NotifyMetricsUnsuspended()=0;
  NS_IMETHOD_(void) NotifyGlyphFragmentTreeSuspended()=0;
  NS_IMETHOD_(void) NotifyGlyphFragmentTreeUnsuspended()=0;
};

#endif // __NS_ISVGGLYPHFRAGMENTNODE_H__
