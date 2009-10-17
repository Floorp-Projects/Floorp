/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "nsSoundPlayer.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsServiceManagerUtils.h"
#include "nsGkAtoms.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsCOMPtr.h"

#ifdef DEBUG
#include "nsPrintfCString.h"
#endif

nsSoundPlayer* nsSoundPlayer::sInstance = nsnull;

NS_IMPL_ISUPPORTS2(nsSoundPlayer, nsISoundPlayer, nsIDOMEventListener)


nsSoundPlayer::nsSoundPlayer()
{
}

nsSoundPlayer::~nsSoundPlayer()
{
  if (this == sInstance) {
    sInstance = nsnull;
  }
#ifdef MOZ_MEDIA
  for (PRInt32 i = mAudioElements.Count() - 1; i >= 0; i--) {
    RemoveEventListeners(mAudioElements[i]);
  }
#endif // MOZ_MEDIA
}

/* static */ nsSoundPlayer*
nsSoundPlayer::GetInstance()
{
  if (!sInstance) {
    sInstance = new nsSoundPlayer();
  }
  NS_IF_ADDREF(sInstance);
  return sInstance;
}

#ifdef MOZ_MEDIA

void
nsSoundPlayer::RemoveEventListeners(nsIDOMHTMLMediaElement *aElement)
{
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(aElement));
  target->RemoveEventListener(NS_LITERAL_STRING("ended"), this, PR_TRUE);
  target->RemoveEventListener(NS_LITERAL_STRING("error"), this, PR_TRUE);
}

nsresult
nsSoundPlayer::CreateAudioElement(nsIDOMHTMLMediaElement **aElement)
{
  nsresult rv;
  nsCOMPtr<nsIDOMHTMLMediaElement> audioElement =
    do_CreateInstance("@mozilla.org/content/element/html;1?name=audio", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(audioElement,
               "do_CreateInterface succeeded, but the result is null");
  audioElement->SetAutoplay(PR_TRUE);
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(audioElement));
  target->AddEventListener(NS_LITERAL_STRING("ended"), this, PR_TRUE);
  target->AddEventListener(NS_LITERAL_STRING("error"), this, PR_TRUE);
  NS_ADDREF(*aElement = audioElement);
  return NS_OK;
}

#endif // MOZ_MEDIA

NS_IMETHODIMP
nsSoundPlayer::Play(nsIURL* aURL)
{
#ifdef MOZ_MEDIA
  nsCAutoString spec;
  nsresult rv = aURL->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  if (spec.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLMediaElement> audioElement;
  rv = CreateAudioElement(getter_AddRefs(audioElement));
  NS_ENSURE_SUCCESS(rv, rv);

  mAudioElements.AppendObject(audioElement);

  rv = audioElement->SetAttribute(NS_LITERAL_STRING("src"),
                                  NS_ConvertUTF8toUTF16(spec));
  NS_ENSURE_SUCCESS(rv, rv);
  audioElement->Load();
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif // MOZ_MEDIA
}

NS_IMETHODIMP
nsSoundPlayer::Stop()
{
#ifdef MOZ_MEDIA
  for (PRInt32 i = mAudioElements.Count() - 1; i >= 0; i--) {
    nsCOMPtr<nsIDOMHTMLMediaElement> audioElement = mAudioElements[i];
    if (!audioElement) {
      NS_WARNING("mAudioElements has null item");
      continue;
    }
    RemoveEventListeners(audioElement);
    audioElement->Pause();
  }
  mAudioElements.Clear();
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif // MOZ_MEDIA
}

NS_IMETHODIMP
nsSoundPlayer::HandleEvent(nsIDOMEvent *aEvent)
{
#ifdef MOZ_MEDIA
  NS_ENSURE_ARG_POINTER(aEvent);

  nsresult rv;
  nsCOMPtr<nsIDOMEventTarget> target;
  rv = aEvent->GetTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(target, NS_OK);

  nsCOMPtr<nsIDOMHTMLMediaElement> audioElement = do_QueryInterface(target);
  NS_ENSURE_TRUE(audioElement, NS_OK);

  RemoveEventListeners(audioElement);

  mAudioElements.RemoveObject(audioElement);

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif // MOZ_MEDIA
}
