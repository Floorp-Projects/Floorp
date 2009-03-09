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

#include "nsError.h"
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
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsContentErrors.h"
#include "nsCrossSiteListenerProxy.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *gFontDownloaderLog = PR_NewLogModule("fontdownloader");
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(gFontDownloaderLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gFontDownloaderLog, PR_LOG_DEBUG)


nsFontFaceLoader::nsFontFaceLoader(gfxFontEntry *aFontToLoad, nsIURI *aFontURI,
                                   nsUserFontSet *aFontSet, nsIChannel *aChannel)
  : mFontEntry(aFontToLoad), mFontURI(aFontURI), mFontSet(aFontSet),
    mChannel(aChannel)
{
}

nsFontFaceLoader::~nsFontFaceLoader()
{
  if (mFontSet) {
    mFontSet->RemoveLoader(this);
  }
}

NS_IMPL_ISUPPORTS1(nsFontFaceLoader, nsIStreamLoaderObserver)

NS_IMETHODIMP
nsFontFaceLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                   nsISupports* aContext,
                                   nsresult aStatus,
                                   PRUint32 aStringLen,
                                   const PRUint8* aString)
{
  if (!mFontSet) {
    // We've been canceled
    return aStatus;
  }

  mFontSet->RemoveLoader(this);

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

  nsPresContext *ctx = mFontSet->GetPresContext();
  NS_ASSERTION(ctx && !ctx->PresShell()->IsDestroying(),
               "We should have been canceled already");

  // whether an error occurred or not, notify the user font set of the completion
  gfxUserFontSet *userFontSet = ctx->GetUserFontSet();
  if (!userFontSet) {
    return aStatus;
  }
  
  PRBool fontUpdate = userFontSet->OnLoadComplete(mFontEntry, aLoader,
                                                  aString, aStringLen,
                                                  aStatus);

  // when new font loaded, need to reflow
  if (fontUpdate) {
    // Update layout for the presence of the new font.  Since this is
    // asynchronous, reflows will coalesce.
    ctx->UserFontSetUpdated();
    LOG(("fontdownloader (%p) reflow\n", this));
  }

  return aStatus;
}

void
nsFontFaceLoader::Cancel()
{
  mFontSet = nsnull;
  mChannel->Cancel(NS_BINDING_ABORTED);
}

nsresult
nsFontFaceLoader::CheckLoadAllowed(nsIPrincipal* aSourcePrincipal,
                                   nsIURI* aTargetURI,
                                   nsISupports* aContext)
{
  nsresult rv;
  
  if (!aSourcePrincipal)
    return NS_OK;
    
  // check content policy
  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_FONT,
                                 aTargetURI,
                                 aSourcePrincipal,
                                 aContext,
                                 EmptyCString(), // mime type
                                 nsnull,
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());

  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    return NS_ERROR_CONTENT_BLOCKED;
  }

  return NS_OK;
}
  
nsUserFontSet::nsUserFontSet(nsPresContext *aContext)
  : mPresContext(aContext)
{
  NS_ASSERTION(mPresContext, "null context passed to nsUserFontSet");
  mLoaders.Init();
}

nsUserFontSet::~nsUserFontSet()
{
  NS_ASSERTION(mLoaders.Count() == 0, "mLoaders should have been emptied");
}

static PLDHashOperator DestroyIterator(nsPtrHashKey<nsFontFaceLoader>* aKey,
                                       void* aUserArg)
{
  aKey->GetKey()->Cancel();
  return PL_DHASH_REMOVE;
}

void
nsUserFontSet::Destroy()
{
  mPresContext = nsnull;
  mLoaders.EnumerateEntries(DestroyIterator, nsnull);
}

void
nsUserFontSet::RemoveLoader(nsFontFaceLoader *aLoader)
{
  mLoaders.RemoveEntry(aLoader);
}

nsresult 
nsUserFontSet::StartLoad(gfxFontEntry *aFontToLoad, 
                          const gfxFontFaceSrc *aFontFaceSrc)
{
  nsresult rv;
  
  // check same-site origin
  nsIPresShell *ps = mPresContext->PresShell();
  if (!ps)
    return NS_ERROR_FAILURE;
    
  NS_ASSERTION(aFontFaceSrc && !aFontFaceSrc->mIsLocal, 
               "bad font face url passed to fontloader");
  NS_ASSERTION(aFontFaceSrc->mURI, "null font uri");
  if (!aFontFaceSrc->mURI)
    return NS_ERROR_FAILURE;

  // use document principal, original principal if flag set
  // this enables user stylesheets to load font files via
  // @font-face rules
  nsCOMPtr<nsIPrincipal> principal = ps->GetDocument()->NodePrincipal();

  NS_ASSERTION(aFontFaceSrc->mOriginPrincipal, 
               "null origin principal in @font-face rule");
  if (aFontFaceSrc->mUseOriginPrincipal) {
    principal = do_QueryInterface(aFontFaceSrc->mOriginPrincipal);
  }
  
  rv = nsFontFaceLoader::CheckLoadAllowed(principal, aFontFaceSrc->mURI, 
                                          ps->GetDocument());
  if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
    if (LOG_ENABLED()) {
      nsCAutoString fontURI, referrerURI;
      aFontFaceSrc->mURI->GetSpec(fontURI);
      if (aFontFaceSrc->mReferrer)
        aFontFaceSrc->mReferrer->GetSpec(referrerURI);
      LOG(("fontdownloader download blocked - font uri: (%s) "
           "referrer uri: (%s) err: %8.8x\n", 
          fontURI.get(), referrerURI.get(), rv));
    }
#endif    
    return rv;
  }

  nsCOMPtr<nsIStreamLoader> streamLoader;
  nsCOMPtr<nsILoadGroup> loadGroup(ps->GetDocument()->GetDocumentLoadGroup());

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     aFontFaceSrc->mURI,
                     nsnull,
                     loadGroup,
                     nsnull,
                     nsIRequest::LOAD_NORMAL);
                     
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsFontFaceLoader> fontLoader =
    new nsFontFaceLoader(aFontToLoad, aFontFaceSrc->mURI, this, channel);

  if (!fontLoader)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef PR_LOGGING
  if (LOG_ENABLED()) {
    nsCAutoString fontURI, referrerURI;
    aFontFaceSrc->mURI->GetSpec(fontURI);
    if (aFontFaceSrc->mReferrer)
      aFontFaceSrc->mReferrer->GetSpec(referrerURI);
    LOG(("fontdownloader (%p) download start - font uri: (%s) "
         "referrer uri: (%s)\n", 
         fontLoader.get(), fontURI.get(), referrerURI.get()));
  }
#endif  

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel)
    httpChannel->SetReferrer(aFontFaceSrc->mReferrer);
  rv = NS_NewStreamLoader(getter_AddRefs(streamLoader), fontLoader);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool inherits = PR_FALSE;
  rv = NS_URIChainHasFlags(aFontFaceSrc->mURI,
                           nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                           &inherits);
  if (NS_SUCCEEDED(rv) && inherits) {
    // allow data, javascript, etc URI's
    rv = channel->AsyncOpen(streamLoader, nsnull);
  } else {
    nsCOMPtr<nsIStreamListener> listener =
      new nsCrossSiteListenerProxy(streamLoader, principal, channel, 
                                   PR_FALSE, &rv);
    if (NS_FAILED(rv)) {
      fontLoader->DropChannel();  // explicitly need to break ref cycle
    }
    NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = channel->AsyncOpen(listener, nsnull);
  }

  if (NS_SUCCEEDED(rv)) {
    mLoaders.PutEntry(fontLoader);
  }

  return rv;
}
