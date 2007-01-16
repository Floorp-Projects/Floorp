/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   robert@ocallahan.org
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef NSTEXTRUNTRANSFORMATIONS_H_
#define NSTEXTRUNTRANSFORMATIONS_H_

#include "gfxFont.h"

class nsCaseTransformingTextRun;
class nsStyleContext;

class nsTransformingTextRunFactory : public gfxTextRunFactory {
public:
  nsTransformingTextRunFactory(gfxTextRunFactory* aInnerTextRunFactory,
                               nsTransformingTextRunFactory* aInnerTransformingTextRunFactory)
    : mInnerTextRunFactory(aInnerTextRunFactory),
      mInnerTransformingTextRunFactory(aInnerTransformingTextRunFactory),
      mStyles(nsnull) {}

  // Assign style contexts to each character in the string. Call this before
  // calling MakeTextRun. The array needs to have a lifetime covering the
  // call to MakeTextRun.
  virtual void SetStyles(nsStyleContext** aStyles) { mStyles = aStyles; }
  // Default 8-bit path just transforms to Unicode and takes that path
  virtual gfxTextRun* MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                  Parameters* aParams);
  // Redeclare this to make C++ compiler shut up
  virtual gfxTextRun* MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                  Parameters* aParams) = 0;

protected:
  nsRefPtr<gfxTextRunFactory>            mInnerTextRunFactory;
  nsRefPtr<nsTransformingTextRunFactory> mInnerTransformingTextRunFactory;
  nsStyleContext**                       mStyles;
};

/**
 * Builds textruns that render their text using a font-variant (i.e.,
 * smallcaps).
 */
class nsFontVariantTextRunFactory : public nsTransformingTextRunFactory {
public:
  nsFontVariantTextRunFactory(gfxFontGroup* aFontGroup)
    : nsTransformingTextRunFactory(aFontGroup, nsnull), mFontGroup(aFontGroup) {}
    
  // Redeclare this to make C++ compiler shut up
  virtual gfxTextRun* MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                  Parameters* aParams) {
    return nsTransformingTextRunFactory::MakeTextRun(aString, aLength, aParams);
  }
  virtual gfxTextRun* MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                  Parameters* aParams);

private:
  nsRefPtr<gfxFontGroup> mFontGroup;
};

/**
 * Builds textruns that transform the text in some way (e.g., capitalize)
 * and then render the text using some other textrun implementation.
 */
class nsCaseTransformTextRunFactory : public nsTransformingTextRunFactory {
public:
  nsCaseTransformTextRunFactory(gfxTextRunFactory* aInnerTextRunFactory,
                                nsTransformingTextRunFactory* aInnerTransformingTextRunFactory,
                                PRBool aAllUppercase = PR_FALSE)
    : nsTransformingTextRunFactory(aInnerTextRunFactory, aInnerTransformingTextRunFactory),
      mAllUppercase(aAllUppercase) {}

  // Redeclare this to make C++ compiler shut up
  virtual gfxTextRun* MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                  Parameters* aParams) {
    return nsTransformingTextRunFactory::MakeTextRun(aString, aLength, aParams);
  }
  virtual gfxTextRun* MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                  Parameters* aParams);

  // We need these for the implementation of case-transforming text runs
  gfxTextRunFactory* GetInnerTextRunFactory() { return mInnerTextRunFactory; }
  nsTransformingTextRunFactory* GetInnerTransformingTextRunFactory() {
    return mInnerTransformingTextRunFactory;
  }
  nsStyleContext** GetStyles() { return mStyles; }
  PRBool IsAllUppercase() { return mAllUppercase; }

private:
  PRPackedBool mAllUppercase;
};

#endif /*NSTEXTRUNTRANSFORMATIONS_H_*/
