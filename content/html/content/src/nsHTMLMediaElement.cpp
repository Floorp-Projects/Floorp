/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
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
#include "nsIDOMHTMLMediaElement.h"
#include "nsIDOMHTMLSourceElement.h"
#include "nsHTMLMediaElement.h"
#include "nsGenericHTMLElement.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMError.h"
#include "nsNodeInfoManager.h"
#include "plbase64.h"
#include "nsNetUtil.h"
#include "prmem.h"
#include "nsNetUtil.h"
#include "nsXPCOMStrings.h"
#include "prlock.h"
#include "nsThreadUtils.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

#include "nsIRenderingContext.h"
#include "nsITimer.h"

#include "nsEventDispatcher.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMProgressEvent.h"
#include "nsHTMLMediaError.h"
#include "nsICategoryManager.h"

#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsContentErrors.h"

#ifdef MOZ_OGG
#include "nsOggDecoder.h"
#endif
#ifdef MOZ_WAVE
#include "nsWaveDecoder.h"
#endif

class nsAsyncEventRunner : public nsRunnable
{
private:
  nsString mName;
  nsCOMPtr<nsHTMLMediaElement> mElement;
  PRPackedBool mProgress;
  
public:
  nsAsyncEventRunner(const nsAString& aName, nsHTMLMediaElement* aElement, PRBool aProgress) : 
    mName(aName), mElement(aElement), mProgress(aProgress)
  {
  }
  
  NS_IMETHOD Run() {
    return mProgress ?
      mElement->DispatchProgressEvent(mName) :
      mElement->DispatchSimpleEvent(mName);
  }
};

// nsIDOMHTMLMediaElement
NS_IMPL_URI_ATTR(nsHTMLMediaElement, Src, src)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Controls, controls)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Autoplay, autoplay)

/* readonly attribute nsIDOMHTMLMediaError error; */
NS_IMETHODIMP nsHTMLMediaElement::GetError(nsIDOMHTMLMediaError * *aError)
{
  NS_IF_ADDREF(*aError = mError);

  return NS_OK;
}

/* readonly attribute boolean ended; */
NS_IMETHODIMP nsHTMLMediaElement::GetEnded(PRBool *aEnded)
{
  *aEnded = mEnded;

  return NS_OK;
}

/* readonly attribute DOMString currentSrc; */
NS_IMETHODIMP nsHTMLMediaElement::GetCurrentSrc(nsAString & aCurrentSrc)
{
  nsCAutoString src;
  
  if (mDecoder) {
    nsCOMPtr<nsIURI> uri;
    mDecoder->GetCurrentURI(getter_AddRefs(uri));
    if (uri) {
      uri->GetSpec(src);
    }
  }

  aCurrentSrc = NS_ConvertUTF8toUTF16(src);

  return NS_OK;
}

/* readonly attribute unsigned short networkState; */
NS_IMETHODIMP nsHTMLMediaElement::GetNetworkState(PRUint16 *aNetworkState)
{
  *aNetworkState = mNetworkState;

  return NS_OK;
}

/* readonly attribute unsigned long totalBytes; */
NS_IMETHODIMP nsHTMLMediaElement::GetTotalBytes(PRUint32 *aTotalBytes)
{
  *aTotalBytes = mDecoder ? PRUint32(mDecoder->GetTotalBytes()) : 0;
  return NS_OK;
}

/* void load (); */
NS_IMETHODIMP nsHTMLMediaElement::Load()
{
  return LoadWithChannel(nsnull, nsnull);
}

nsresult nsHTMLMediaElement::LoadWithChannel(nsIChannel *aChannel,
                                             nsIStreamListener **aListener)
{
  NS_ASSERTION((aChannel == nsnull) == (aListener == nsnull),
               "channel and listener should both be null or both non-null");

  if (aListener) {
    *aListener = nsnull;
  }

  if (mBegun) {
    mBegun = PR_FALSE;
    
    mError = new nsHTMLMediaError(nsIDOMHTMLMediaError::MEDIA_ERR_ABORTED);
    DispatchProgressEvent(NS_LITERAL_STRING("abort"));
    return NS_OK;
  }

  mError = nsnull;
  mLoadedFirstFrame = PR_FALSE;
  mAutoplaying = PR_TRUE;

  // TODO: The playback rate must be set to the default playback rate.

  if (mNetworkState != nsIDOMHTMLMediaElement::EMPTY) {
    mNetworkState = nsIDOMHTMLMediaElement::EMPTY;
    ChangeReadyState(nsIDOMHTMLMediaElement::DATA_UNAVAILABLE);
    mPaused = PR_TRUE;
    // TODO: The current playback position must be set to 0.
    // TODO: The currentLoop DOM attribute must be set to 0.
    DispatchSimpleEvent(NS_LITERAL_STRING("emptied"));
  }

  nsresult rv;
  if (aChannel) {
    rv = InitializeDecoderForChannel(aChannel, aListener);
  } else {
    rv = PickMediaElement();
  }
  NS_ENSURE_SUCCESS(rv, rv);

  mBegun = PR_TRUE;
  mEnded = PR_FALSE;

  DispatchAsyncProgressEvent(NS_LITERAL_STRING("loadstart"));

  return NS_OK;
}

/* readonly attribute unsigned short readyState; */
NS_IMETHODIMP nsHTMLMediaElement::GetReadyState(PRUint16 *aReadyState)
{
  *aReadyState = mReadyState;

  return NS_OK;
}

/* readonly attribute boolean seeking; */
NS_IMETHODIMP nsHTMLMediaElement::GetSeeking(PRBool *aSeeking)
{
  *aSeeking = mDecoder && mDecoder->IsSeeking();

  return NS_OK;
}

/* attribute float currentTime; */
NS_IMETHODIMP nsHTMLMediaElement::GetCurrentTime(float *aCurrentTime)
{
  *aCurrentTime = mDecoder ? mDecoder->GetCurrentTime() : 0.0;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetCurrentTime(float aCurrentTime)
{
  if (!mDecoder)
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  // Detect for a NaN and invalid values.
  if (!(aCurrentTime >= 0.0))
    return NS_ERROR_FAILURE;

  if (mNetworkState < nsIDOMHTMLMediaElement::LOADED_METADATA) 
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  mPlayingBeforeSeek = IsActivelyPlaying();
  // The media backend is responsible for dispatching the timeupdate
  // event if it changes the playback position as a result of the seek.
  nsresult rv = mDecoder->Seek(aCurrentTime);
  return rv;
}

/* readonly attribute float duration; */
NS_IMETHODIMP nsHTMLMediaElement::GetDuration(float *aDuration)
{
  *aDuration =  mDecoder ? mDecoder->GetDuration() : 0.0;
  return NS_OK;
}

/* readonly attribute boolean paused; */
NS_IMETHODIMP nsHTMLMediaElement::GetPaused(PRBool *aPaused)
{
  *aPaused = mPaused;

  return NS_OK;
}

/* void pause (); */
NS_IMETHODIMP nsHTMLMediaElement::Pause()
{
  if (!mDecoder) 
    return NS_OK;

  nsresult rv;

  if (mNetworkState == nsIDOMHTMLMediaElement::EMPTY) {
    rv = Load();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mDecoder->Pause();
  PRBool oldPaused = mPaused;
  mPaused = PR_TRUE;
  mAutoplaying = PR_FALSE;
  
  if (!oldPaused) {
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("timeupdate"));
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("pause"));
  }

  return NS_OK;
}

/* attribute float volume; */
NS_IMETHODIMP nsHTMLMediaElement::GetVolume(float *aVolume)
{
  if (mMuted)
    *aVolume = mMutedVolume;
  else
    *aVolume = mDecoder ? mDecoder->GetVolume() : 0.0;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetVolume(float aVolume)
{
  if (aVolume < 0.0f || aVolume > 1.0f)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  if (mMuted) 
    mMutedVolume = aVolume;
  else {
    if (mDecoder)
      mDecoder->SetVolume(aVolume);

    DispatchSimpleEvent(NS_LITERAL_STRING("volumechange"));
  }
  return NS_OK;
}

/* attribute boolean muted; */
NS_IMETHODIMP nsHTMLMediaElement::GetMuted(PRBool *aMuted)
{
  *aMuted = mMuted;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetMuted(PRBool aMuted)
{
  PRBool oldMuted = mMuted;

  if (mDecoder) {
    if (mMuted && !aMuted) {
      mDecoder->SetVolume(mMutedVolume);
    }
    else if (!mMuted && aMuted) {
      mMutedVolume = mDecoder->GetVolume();
      mDecoder->SetVolume(0.0);
    }
  }

  mMuted = aMuted;

  if (oldMuted != mMuted) 
    DispatchSimpleEvent(NS_LITERAL_STRING("volumechange"));
  return NS_OK;
}

nsHTMLMediaElement::nsHTMLMediaElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
  : nsGenericHTMLElement(aNodeInfo),
    mNetworkState(nsIDOMHTMLMediaElement::EMPTY),
    mReadyState(nsIDOMHTMLMediaElement::DATA_UNAVAILABLE),
    mMutedVolume(0.0),
    mMediaSize(-1,-1),
    mBegun(PR_FALSE),
    mEnded(PR_FALSE),
    mLoadedFirstFrame(PR_FALSE),
    mAutoplaying(PR_TRUE),
    mPaused(PR_TRUE),
    mMuted(PR_FALSE),
    mIsDoneAddingChildren(!aFromParser),
    mPlayingBeforeSeek(PR_FALSE)
{
}

nsHTMLMediaElement::~nsHTMLMediaElement()
{
  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nsnull;
  }
}

NS_IMETHODIMP
nsHTMLMediaElement::Play(void)
{
  if (!mDecoder)
    return NS_OK;

  nsresult rv;

  if (mNetworkState == nsIDOMHTMLMediaElement::EMPTY) {
    rv = Load();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mEnded) {
    mEnded = PR_FALSE;
    SetCurrentTime(0);
  }

  // TODO: If the playback has ended, then the user agent must set 
  // currentLoop to zero and seek to the effective start.
  // TODO: The playback rate must be set to the default playback rate.
  rv = mDecoder->Play();
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool oldPaused = mPaused;
  mPaused = PR_FALSE;
  mAutoplaying = PR_FALSE;

  if (oldPaused)
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("play"));

  return NS_OK;
}

PRBool
nsHTMLMediaElement::ParseAttribute(PRInt32 aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::src) {
      static const char* kWhitespace = " \n\r\t\b";
      aResult.SetTo(nsContentUtils::TrimCharsInSet(kWhitespace, aValue));
      return PR_TRUE;
    }
    else if (aAttribute == nsGkAtoms::loopstart
            || aAttribute == nsGkAtoms::loopend
            || aAttribute == nsGkAtoms::start
            || aAttribute == nsGkAtoms::end) {
      return aResult.ParseFloatValue(aValue);
    }
    else if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return PR_TRUE;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsresult
nsHTMLMediaElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            nsIAtom* aPrefix, const nsAString& aValue,
                            PRBool aNotify)
{
  nsresult rv = 
    nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                    aNotify);
  if (aNotify && aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::src) {
      Load();
    }
  }

  return rv;
}

nsresult nsHTMLMediaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                        nsIContent* aBindingParent,
                                        PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, 
                                                 aParent, 
                                                 aBindingParent, 
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mIsDoneAddingChildren &&
      mNetworkState == nsIDOMHTMLMediaElement::EMPTY) {
    Load();
  }

  return rv;
}

void nsHTMLMediaElement::UnbindFromTree(PRBool aDeep,
                                        PRBool aNullParent)
{
  if (!mPaused && mNetworkState != nsIDOMHTMLMediaElement::EMPTY)
    Pause();

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

#ifdef MOZ_OGG
static const char gOggTypes[][16] = {
  "video/ogg",
  "audio/ogg",
  "application/ogg"
};

static PRBool IsOggType(const nsACString& aType)
{
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gOggTypes); ++i) {
    if (aType.EqualsASCII(gOggTypes[i]))
      return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

#ifdef MOZ_WAVE
static const char gWaveTypes[][16] = {
  "audio/x-wav",
  "audio/wav",
  "audio/wave",
  "audio/x-pn-wav"
};

static PRBool IsWaveType(const nsACString& aType)
{
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gWaveTypes); ++i) {
    if (aType.EqualsASCII(gWaveTypes[i]))
      return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

/* static */
PRBool nsHTMLMediaElement::CanHandleMediaType(const char* aMIMEType)
{
#ifdef MOZ_OGG
  if (IsOggType(nsDependentCString(aMIMEType)))
    return PR_TRUE;
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(nsDependentCString(aMIMEType)))
    return PR_TRUE;
#endif
  return PR_FALSE;
}

/* static */
void nsHTMLMediaElement::InitMediaTypes()
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
#ifdef MOZ_OGG
    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gOggTypes); i++) {
      catMan->AddCategoryEntry("Gecko-Content-Viewers", gOggTypes[i],
                               "@mozilla.org/content/document-loader-factory;1",
                               PR_FALSE, PR_TRUE, nsnull);
    }
#endif
#ifdef MOZ_WAVE
    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gWaveTypes); i++) {
      catMan->AddCategoryEntry("Gecko-Content-Viewers", gWaveTypes[i],
                               "@mozilla.org/content/document-loader-factory;1",
                               PR_FALSE, PR_TRUE, nsnull);
    }
#endif
  }
}

/* static */
void nsHTMLMediaElement::ShutdownMediaTypes()
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
#ifdef MOZ_OGG
    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gOggTypes); i++) {
      catMan->DeleteCategoryEntry("Gecko-Content-Viewers", gOggTypes[i], PR_FALSE);
    }
#endif
#ifdef MOZ_WAVE
    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(gWaveTypes); i++) {
      catMan->DeleteCategoryEntry("Gecko-Content-Viewers", gWaveTypes[i], PR_FALSE);
    }
#endif
  }
}

PRBool nsHTMLMediaElement::CreateDecoder(const nsACString& aType)
{
#ifdef MOZ_OGG
  if (IsOggType(aType)) {
    mDecoder = new nsOggDecoder();
    if (mDecoder && !mDecoder->Init()) {
      mDecoder = nsnull;
    }
  }
#endif
#ifdef MOZ_WAVE
  if (IsWaveType(aType)) {
    mDecoder = new nsWaveDecoder();
    if (mDecoder && !mDecoder->Init()) {
      mDecoder = nsnull;
    }
  }
#endif
  return mDecoder != nsnull;
}

nsresult nsHTMLMediaElement::InitializeDecoderForChannel(nsIChannel *aChannel,
                                                         nsIStreamListener **aListener)
{
  nsCAutoString mimeType;
  aChannel->GetContentType(mimeType);

  if (!CreateDecoder(mimeType))
    return NS_ERROR_FAILURE;

  mNetworkState = nsIDOMHTMLMediaElement::LOADING;
  mDecoder->ElementAvailable(this);
  
  return mDecoder->Load(nsnull, aChannel, aListener);
}

nsresult nsHTMLMediaElement::PickMediaElement()
{
  // Implements:
  // http://www.whatwg.org/specs/web-apps/current-work/#pick-a
  nsAutoString src;
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
#ifdef MOZ_OGG
    // Currently assuming an Ogg file
    // TODO: Instantiate decoder based on type
    if (mDecoder) {
      mDecoder->ElementUnavailable();
      mDecoder->Shutdown();
      mDecoder = nsnull;
    }

    mDecoder = new nsOggDecoder();
    if (mDecoder && !mDecoder->Init()) {
      mDecoder = nsnull;
    }
#endif
    return InitializeDecoder(src);
  }

  // Checking of 'source' elements as per:
  // http://www.whatwg.org/specs/web-apps/current-work/#pick-a
  PRUint32 count = GetChildCount();
  for (PRUint32 i = 0; i < count; ++i) {
    nsIContent* child = GetChildAt(i);
    NS_ASSERTION(child, "GetChildCount lied!");
    
    nsCOMPtr<nsIContent> source = do_QueryInterface(child);
    if (source) {
      nsAutoString type;
      nsAutoString src;
      if (source->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src) &&
          source->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type) &&
          CreateDecoder(NS_ConvertUTF16toUTF8(type)))
        return InitializeDecoder(src);
    }
  }

  return NS_ERROR_DOM_INVALID_STATE_ERR;
}

nsresult nsHTMLMediaElement::InitializeDecoder(const nsAString& aURISpec)
{
  mNetworkState = nsIDOMHTMLMediaElement::LOADING;

  nsCOMPtr<nsIDocument> doc = GetOwnerDoc();
  if (!doc) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIURI> baseURL = GetBaseURI();
  rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                                 aURISpec,
                                                 doc,
                                                 baseURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> elementPrincipal = NodePrincipal();
  NS_ENSURE_TRUE(elementPrincipal, NS_ERROR_FAILURE);

  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_MEDIA,
                                 uri,
                                 elementPrincipal,
                                 this,
                                 EmptyCString(), // mime type
                                 nsnull, // extra
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    return NS_ERROR_CONTENT_BLOCKED;
  }

  if (mDecoder) {
    mDecoder->ElementAvailable(this);
    rv = mDecoder->Load(uri, nsnull, nsnull);
    if (NS_FAILED(rv)) {
      mDecoder = nsnull;
    }
  }
  return rv;
}

void nsHTMLMediaElement::MetadataLoaded()
{
  mNetworkState = nsIDOMHTMLMediaElement::LOADED_METADATA;
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("durationchange"));
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("loadedmetadata"));
  // TODO: Seek to the start time, as set in the start attribute.
  mDecoder->Seek(0.0);
}

void nsHTMLMediaElement::FirstFrameLoaded()
{
  mNetworkState = nsIDOMHTMLMediaElement::LOADED_FIRST_FRAME;
  ChangeReadyState(nsIDOMHTMLMediaElement::CAN_SHOW_CURRENT_FRAME);
  mLoadedFirstFrame = PR_TRUE;
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("loadedfirstframe"));
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canshowcurrentframe"));
}

void nsHTMLMediaElement::ResourceLoaded()
{
  mBegun = PR_FALSE;
  mEnded = PR_FALSE;
  mNetworkState = nsIDOMHTMLMediaElement::LOADED;
  ChangeReadyState(nsIDOMHTMLMediaElement::CAN_PLAY_THROUGH);

  DispatchProgressEvent(NS_LITERAL_STRING("load"));
}

void nsHTMLMediaElement::NetworkError()
{
  mError = new nsHTMLMediaError(nsIDOMHTMLMediaError::MEDIA_ERR_NETWORK);
  mBegun = PR_FALSE;
  DispatchProgressEvent(NS_LITERAL_STRING("error"));
  mNetworkState = nsIDOMHTMLMediaElement::EMPTY;
  DispatchSimpleEvent(NS_LITERAL_STRING("empty"));
}

void nsHTMLMediaElement::PlaybackEnded()
{
  mBegun = PR_FALSE;
  mEnded = PR_TRUE;
  mPaused = PR_TRUE;
  DispatchSimpleEvent(NS_LITERAL_STRING("ended"));
}

void nsHTMLMediaElement::CanPlayThrough()
{
  ChangeReadyState(nsIDOMHTMLMediaElement::CAN_PLAY_THROUGH);
}

void nsHTMLMediaElement::SeekStarted()
{
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("seeking"));
}

void nsHTMLMediaElement::SeekCompleted()
{
  mPlayingBeforeSeek = PR_FALSE;
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("seeked"));
}

PRBool nsHTMLMediaElement::ShouldCheckAllowOrigin()
{
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1");
  PRBool checkOrigin = PR_TRUE;
  if (prefs) {
    nsCOMPtr<nsIPrefBranch> prefBranch;
    prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
    if (prefBranch) {
      nsresult res = prefBranch->GetBoolPref("browser.media.enforce_same_site_origin",
                                             &checkOrigin);
      if (NS_FAILED(res))
        return PR_TRUE;
    }
  }
  return checkOrigin;
}

void nsHTMLMediaElement::ChangeReadyState(nsMediaReadyState aState)
{
  // Handle raising of "waiting" event during seek (see 4.7.10.8)
  if (mPlayingBeforeSeek && aState <= nsIDOMHTMLMediaElement::CAN_PLAY)
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("waiting"));
    
  mReadyState = aState;
  if (mNetworkState != nsIDOMHTMLMediaElement::EMPTY) {
    switch(mReadyState) {
    case nsIDOMHTMLMediaElement::DATA_UNAVAILABLE:
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("dataunavailable"));
      LOG(PR_LOG_DEBUG, ("Ready state changed to DATA_UNAVAILABLE"));
      break;
      
    case nsIDOMHTMLMediaElement::CAN_SHOW_CURRENT_FRAME:
      if (mLoadedFirstFrame) {
        DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canshowcurrentframe"));
        LOG(PR_LOG_DEBUG, ("Ready state changed to CAN_SHOW_CURRENT_FRAME"));
      }
      break;

    case nsIDOMHTMLMediaElement::CAN_PLAY:
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canplay"));
      LOG(PR_LOG_DEBUG, ("Ready state changed to CAN_PLAY"));
      break;

    case nsIDOMHTMLMediaElement::CAN_PLAY_THROUGH:
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canplaythrough"));
      if (mAutoplaying && 
         mPaused && 
         HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay)) {
        mPaused = PR_FALSE;
        if (mDecoder) {
          mDecoder->Play();
        }
        LOG(PR_LOG_DEBUG, ("Ready state changed to CAN_PLAY_THROUGH"));
        DispatchAsyncSimpleEvent(NS_LITERAL_STRING("play"));
      }
      break;
    }
  }  
}

void nsHTMLMediaElement::Paint(gfxContext* aContext, const gfxRect& aRect) 
{
  if (mDecoder)
    mDecoder->Paint(aContext, aRect);
}

nsresult nsHTMLMediaElement::DispatchSimpleEvent(const nsAString& aName)
{
  return nsContentUtils::DispatchTrustedEvent(GetOwnerDoc(), 
                                              static_cast<nsIContent*>(this), 
                                              aName, 
                                              PR_TRUE, 
                                              PR_TRUE);
}

nsresult nsHTMLMediaElement::DispatchAsyncSimpleEvent(const nsAString& aName)
{
  nsCOMPtr<nsIRunnable> event = new nsAsyncEventRunner(aName, this, PR_FALSE);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL); 
  return NS_OK;                           
}

nsresult nsHTMLMediaElement::DispatchAsyncProgressEvent(const nsAString& aName)
{
  nsCOMPtr<nsIRunnable> event = new nsAsyncEventRunner(aName, this, PR_TRUE);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL); 
  return NS_OK;                           
}

nsresult nsHTMLMediaElement::DispatchProgressEvent(const nsAString& aName)
{
  if (!mDecoder)
    return NS_OK;

  nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(GetOwnerDoc()));
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(static_cast<nsIContent*>(this)));
  NS_ENSURE_TRUE(docEvent && target, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = docEvent->CreateEvent(NS_LITERAL_STRING("ProgressEvent"), getter_AddRefs(event));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMProgressEvent> progressEvent(do_QueryInterface(event));
  NS_ENSURE_TRUE(progressEvent, NS_ERROR_FAILURE);
  
  rv = progressEvent->InitProgressEvent(aName, PR_TRUE, PR_TRUE, PR_FALSE, mDecoder->GetBytesLoaded(), mDecoder->GetTotalBytes());
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool dummy;
  return target->DispatchEvent(event, &dummy);  
}

nsresult nsHTMLMediaElement::DoneAddingChildren(PRBool aHaveNotified)
{
  if (!mIsDoneAddingChildren) {
    mIsDoneAddingChildren = PR_TRUE;
  
    if (mNetworkState == nsIDOMHTMLMediaElement::EMPTY) {
      Load();
    }
  }

  return NS_OK;
}

PRBool nsHTMLMediaElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}

PRBool nsHTMLMediaElement::IsActivelyPlaying() const
{
  // TODO: 
  //   playback has not stopped due to errors, 
  //   and the element has not paused for user interaction
  return 
    !mPaused && 
    (mReadyState == nsIDOMHTMLMediaElement::CAN_PLAY || 
     mReadyState == nsIDOMHTMLMediaElement::CAN_PLAY_THROUGH) &&
    !IsPlaybackEnded();
}
PRBool nsHTMLMediaElement::IsPlaybackEnded() const
{
  // TODO:
  //   the current playback position is equal to the effective end of the media resource, 
  //   and the currentLoop attribute is equal to playCount-1. 
  //   See bug 449157.
  return mNetworkState >= nsIDOMHTMLMediaElement::LOADED_METADATA && mEnded;
}

nsIPrincipal*
nsHTMLMediaElement::GetCurrentPrincipal()
{
  if (!mDecoder)
    return nsnull;

  return mDecoder->GetCurrentPrincipal();
}

void nsHTMLMediaElement::UpdateMediaSize(nsIntSize size)
{
  mMediaSize = size;
}

void nsHTMLMediaElement::DestroyContent()
{
  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nsnull;
  }
  nsGenericHTMLElement::DestroyContent();
}

void nsHTMLMediaElement::Freeze()
{
  mPausedBeforeFreeze = mPaused;
  if (!mPaused) {
    Pause();
  }
}

void nsHTMLMediaElement::Thaw()
{
  if (!mPausedBeforeFreeze) {
    Play();
  }
}
