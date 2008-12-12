/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Daggett <jdaggett@mozilla.com>
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

/* code for loading in @font-face defined font data */

#ifndef nsFontFaceLoader_h_
#define nsFontFaceLoader_h_

#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIStreamLoader.h"
#include "nsIURI.h"
#include "gfxUserFontSet.h"

class nsIRequest;
class nsISupports;
class nsIPresShell;
class nsPresContext;
class nsIPrincipal;

class nsFontFaceLoader : public nsIStreamLoaderObserver
{
public:

  nsFontFaceLoader(gfxFontEntry *aFontToLoad, nsIURI *aFontURI, 
                   nsIPresShell *aShell);
  virtual ~nsFontFaceLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER 

  // initiate the load
  nsresult Init();  

  static nsresult CheckLoadAllowed(nsIPrincipal* aSourcePrincipal,
                                   nsIURI* aTargetURI,
                                   nsISupports* aContext);
  
private:

  nsRefPtr<gfxFontEntry>  mFontEntry;
  nsCOMPtr<nsIURI>        mFontURI;
  nsCOMPtr<nsIPresShell>  mShell;
};

// nsUserFontSet - defines the loading mechanism for downloadable fonts

class nsUserFontSet : public gfxUserFontSet
{
public:
  nsUserFontSet(nsPresContext *aContext);
  ~nsUserFontSet();
  
  // starts loading process, creating and initializing a nsFontFaceLoader obj
  // returns whether load process successfully started or not
  nsresult StartLoad(gfxFontEntry *aFontToLoad, 
                     const gfxFontFaceSrc *aFontFaceSrc);
protected:
  nsPresContext *mPresContext;  // weak reference
};

#endif /* !defined(nsFontFaceLoader_h_) */
