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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Dainis Jonitis, <Dainis_Jonitis@swh-t.lv>.
 * Portions created by the Initial Developer are Copyright (C) 2001
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


#ifndef _nsRegionOS2_h
#define _nsRegionOS2_h

#include "nsiRegion.h"
#include "nsRegion.h"
#define INCL_GPI
#include <os2.h>

class nsRegionOS2 : public nsIRegion
{
  PRUint32 NumOfRects (HPS aPS, HRGN aRegion) const;

public:
  nsRegionOS2 ();

  NS_DECL_ISUPPORTS

  virtual nsresult Init();

  virtual void SetTo(const nsIRegion &aRegion);
  virtual void SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual void Intersect(const nsIRegion &aRegion);
  virtual void Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual void Union(const nsIRegion &aRegion);
  virtual void Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual void Subtract(const nsIRegion &aRegion);
  virtual void Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual PRBool IsEmpty(void);
  virtual PRBool IsEqual(const nsIRegion &aRegion);
  virtual void GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight);
  virtual void Offset(PRInt32 aXOffset, PRInt32 aYOffset);
  virtual PRBool ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD GetRects(nsRegionRectSet **aRects);
  NS_IMETHOD FreeRects(nsRegionRectSet *aRects);
  // Don't use this on OS/2 - Use GetHRGN () instead	
  NS_IMETHOD GetNativeRegion(void *&aRegion) const;
  NS_IMETHOD GetRegionComplexity(nsRegionComplexity &aComplexity) const;
  NS_IMETHOD GetNumRects(PRUint32 *aRects) const;
 
  // OS/2 specific
  // get region in widget's coord space for given device
  HRGN GetHRGN (PRUint32 ulHeight, HPS hps);

  // copy from another region defined in aWidget's space for a given device
  nsresult InitWithHRGN (HRGN copy, PRUint32 ulHeight, HPS hps);
private:
  nsRegion mRegion;
};

#endif
