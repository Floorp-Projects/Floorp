/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 05/08/2000       IBM Corp.      Fix for trying to us an already freed mGammaTable.
 */

// Manage picking of colours via various GPI methods.
// Created & (hopefully) destroyed by nsDeviceContextOS2; each rendering
// context spun off from that dc holds a ref to it, and uses it to get things
// right.  Gamma correction done here too using the dc's table, so don't
// go through gamma before GetGPIColor()'ing.
//
// !! What to do about A-channel ?

#ifndef _nspaletteos2_h
#define _nspaletteos2_h

#include "nsIDeviceContext.h"
#include "nscolor.h"

class nsIDeviceContext;

class nsIPaletteOS2 : public nsISupports
{
 public:
   virtual long     GetGPIColor( nsIDeviceContext *aContext, HPS hps, nscolor rgb) = 0;
   virtual nsresult Select( HPS hps, nsIDeviceContext *aContext) = 0;
   virtual nsresult Deselect( HPS hps) = 0;
   virtual nsresult GetNSPalette( nsPalette &aPalette) const = 0;
};

// So yes, this could be an nsDeviceContextOS2 method, but this way is better
// for modularisation.  Oh yes.
// Release when done.
nsresult NS_CreatePalette( nsIDeviceContext *aContext,
                           nsIPaletteOS2 *&aPalette);

#endif
