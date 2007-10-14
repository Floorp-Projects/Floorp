/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
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
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
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

#include <strings.h>

#include "nsIURI.h"

#include "EmbedContentListener.h"
#include "EmbedPrivate.h"

#include "nsServiceManagerUtils.h"
#include "nsIWebNavigationInfo.h"
#include "nsDocShellCID.h"

EmbedContentListener::EmbedContentListener(void)
{
  mOwner = nsnull;
}

EmbedContentListener::~EmbedContentListener()
{
}

NS_IMPL_ISUPPORTS2(EmbedContentListener,
                   nsIURIContentListener,
                   nsISupportsWeakReference)

nsresult
EmbedContentListener::Init(EmbedPrivate *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::OnStartURIOpen(nsIURI     *aURI,
             PRBool     *aAbortOpen)
{
  nsresult rv;

  if (mOwner->mOpenBlock) {
    *aAbortOpen = mOwner->mOpenBlock;
    mOwner->mOpenBlock = PR_FALSE;
    return NS_OK;
  }
  nsCAutoString specString;
  rv = aURI->GetSpec(specString);

  if (NS_FAILED(rv))
    return rv;

  gint return_val = FALSE;

  /* checks if URI scheme is mailto */
  aURI->SchemeIs("mailto", &return_val);
  if (return_val) {
    /* stops URI opening to emit "mailto" signal */
    *aAbortOpen = TRUE;

    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                    moz_embed_signals[MAILTO],
                    specString.get());

    return NS_OK;
  }

  // otherwise  ...
  return_val = FALSE;

  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[OPEN_URI],
                  specString.get(), &return_val);

  *aAbortOpen = !!return_val;

  return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::DoContent(const char         *aContentType,
                                PRBool             aIsContentPreferred,
                                nsIRequest         *aRequest,
                                nsIStreamListener **aContentHandler,
                                PRBool             *aAbortProcess)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::IsPreferred(const char        *aContentType,
                                  char             **aDesiredContentType,
                                  PRBool            *aCanHandleContent)
{
  return CanHandleContent(aContentType, PR_TRUE, aDesiredContentType,
                          aCanHandleContent);
}

NS_IMETHODIMP
EmbedContentListener::CanHandleContent(const char        *aContentType,
                                       PRBool           aIsContentPreferred,
                                       char             **aDesiredContentType,
                                       PRBool            *_retval)
{
  *_retval = PR_FALSE;
  *aDesiredContentType = nsnull;

  if (aContentType) {
    nsCOMPtr<nsIWebNavigationInfo> webNavInfo(
        do_GetService(NS_WEBNAVIGATION_INFO_CONTRACTID));
    if (webNavInfo) {
      PRUint32 canHandle;
      nsresult rv =
        webNavInfo->IsTypeSupported(nsDependentCString(aContentType),
                                    mOwner ? mOwner->mNavigation.get() : nsnull,
                                    &canHandle); //FIXME XXX MEMLEAK
      NS_ENSURE_SUCCESS(rv, rv);
      *_retval = (canHandle != nsIWebNavigationInfo::UNSUPPORTED);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::GetLoadCookie(nsISupports **aLoadCookie)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::SetLoadCookie(nsISupports *aLoadCookie)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::GetParentContentListener(nsIURIContentListener **aParent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::SetParentContentListener(nsIURIContentListener *aParent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

