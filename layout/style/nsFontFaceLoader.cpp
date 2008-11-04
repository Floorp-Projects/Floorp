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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "nsFontFaceLoader.h"

#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIStreamListener.h"
#include "nsNetUtil.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsContentUtils.h"

#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"


#ifdef PR_LOGGING
static PRLogModuleInfo *gFontDownloaderLog = PR_NewLogModule("fontdownloader");
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(gFontDownloaderLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gFontDownloaderLog, PR_LOG_DEBUG)


#define ENFORCE_SAME_SITE_ORIGIN "gfx.downloadable_fonts.enforce_same_site_origin"
static PRBool gEnforceSameSiteOrigin = PR_TRUE;

static PRBool
CheckMayLoad(nsIDocument* aDoc, nsIURI* aURI)
{
  // allow this to be disabled via a pref
  static PRBool init = PR_FALSE;

  if (!init) {
    init = PR_TRUE;
    nsContentUtils::AddBoolPrefVarCache(ENFORCE_SAME_SITE_ORIGIN, &gEnforceSameSiteOrigin);
  }

  if (!gEnforceSameSiteOrigin)
    return PR_TRUE;

  if (!aDoc)
    return PR_FALSE;

  nsresult rv = aDoc->NodePrincipal()->CheckMayLoad(aURI, PR_TRUE);
  return NS_SUCCEEDED(rv);
}


nsFontFaceLoader::nsFontFaceLoader(gfxFontEntry *aFontToLoad, nsIURI *aFontURI,
                                   gfxUserFontSet::LoaderContext *aContext)
  : mFontEntry(aFontToLoad), mFontURI(aFontURI), mLoaderContext(aContext)
{

}

nsFontFaceLoader::~nsFontFaceLoader()
{

}

NS_IMPL_ISUPPORTS1(nsFontFaceLoader, nsIStreamLoaderObserver)

NS_IMETHODIMP
nsFontFaceLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                   nsISupports* aContext,
                                   nsresult aStatus,
                                   PRUint32 aStringLen,
                                   const PRUint8* aString)
{

#ifdef PR_LOGGING
  if (LOG_ENABLED()) {
    nsCAutoString fontURI;
    mFontURI->GetSpec(fontURI);
    if (NS_SUCCEEDED(aStatus)) {
      LOG(("fontdownloader (%p) download completed - font uri: (%s)\n", 
           this, fontURI.get()));
    } else {
      LOG(("fontdownloader (%p) download failed - font uri: (%s) error: %8.8x\n", 
           this, fontURI.get(), aStatus));
    }
  }
#endif

  PRBool fontUpdate;

  // whether an error occurred or not, notify the user font set of the completion
  fontUpdate = mLoaderContext->mUserFontSet->OnLoadComplete(mFontEntry, 
                                                            aString, aStringLen,
                                                            aStatus);

  // when new font loaded, need to reflow
  if (fontUpdate) {
    nsFontFaceLoaderContext *loaderCtx 
                       = static_cast<nsFontFaceLoaderContext*> (mLoaderContext);

    nsIPresShell *ps = loaderCtx->mPresContext->PresShell();
    if (ps) {
      // reflow async so that reflows coalesce
      ps->StyleChangeReflow();
      LOG(("fontdownloader (%p) reflow\n", this));
    }
  }

  return aStatus;
}

PRBool
nsFontFaceLoader::CreateHandler(gfxFontEntry *aFontToLoad, nsIURI *aFontURI, 
                                gfxUserFontSet::LoaderContext *aContext)
{
  // check same-site origin
  nsFontFaceLoaderContext *loaderCtx 
                             = static_cast<nsFontFaceLoaderContext*> (aContext);

  nsIPresShell *ps = loaderCtx->mPresContext->PresShell();
  if (!ps)
    return PR_FALSE;

  if (!CheckMayLoad(ps->GetDocument(), aFontURI))
    return PR_FALSE;

  nsRefPtr<nsFontFaceLoader> fontLoader = new nsFontFaceLoader(aFontToLoad, 
                                                               aFontURI, 
                                                               aContext);
  if (!fontLoader)
    return PR_FALSE;

#ifdef PR_LOGGING
  if (LOG_ENABLED()) {
    nsCAutoString fontURI;
    aFontURI->GetSpec(fontURI);
    LOG(("fontdownloader (%p) download start - font uri: (%s)\n", 
         fontLoader.get(), fontURI.get()));
  }
#endif  

  nsCOMPtr<nsIStreamLoader> streamLoader;
  nsCOMPtr<nsILoadGroup> loadGroup(ps->GetDocument()->GetDocumentLoadGroup());
  nsCOMPtr<nsIInterfaceRequestor> sameOriginChecker 
                                       = nsContentUtils::GetSameOriginChecker();

  nsresult rv = NS_NewStreamLoader(getter_AddRefs(streamLoader), aFontURI, 
                                   fontLoader, nsnull, loadGroup, 
                                   sameOriginChecker);

  return NS_SUCCEEDED(rv);
}

