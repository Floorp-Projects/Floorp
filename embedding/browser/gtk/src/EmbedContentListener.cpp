/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include <strings.h>
#include <nsXPIDLString.h>

#include "nsIURI.h"

#include "EmbedContentListener.h"
#include "EmbedPrivate.h"

EmbedContentListener::EmbedContentListener(void)
{
  NS_INIT_REFCNT();
  mOwner = nsnull;
}

EmbedContentListener::~EmbedContentListener()
{
}

NS_IMPL_ISUPPORTS1(EmbedContentListener,
		   nsIURIContentListener)

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

  nsXPIDLCString specString;
  rv = aURI->GetSpec(getter_Copies(specString));

  if (NS_FAILED(rv))
    return rv;

  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[OPEN_URI],
		  (const char *)specString, &return_val);

  *aAbortOpen = return_val;

  return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::DoContent(const char         *aContentType,
				nsURILoadCommand    aCommand,
				nsIRequest         *aRequest,
				nsIStreamListener **aContentHandler,
				PRBool             *aAbortProcess)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::IsPreferred(const char        *aContentType,
				  nsURILoadCommand   aCommand,
				  char             **aDesiredContentType,
				  PRBool            *aCanHandleContent)
{
  if (aContentType &&
      (!strcasecmp(aContentType, "text/html")   ||
       !strcasecmp(aContentType, "text/plain")  ||
       !strcasecmp(aContentType, "application/vnd.mozilla.xul+xml")    ||
       !strcasecmp(aContentType, "text/rdf")    ||
       !strcasecmp(aContentType, "text/xml")    ||
       !strcasecmp(aContentType, "application/xml")    ||
       !strcasecmp(aContentType, "application/xhtml+xml")    ||
       !strcasecmp(aContentType, "text/css")    ||
       !strcasecmp(aContentType, "image/gif")   ||
       !strcasecmp(aContentType, "image/jpeg")  ||
       !strcasecmp(aContentType, "image/png")   ||
       !strcasecmp(aContentType, "image/tiff")  ||
       !strcasecmp(aContentType, "application/http-index-format"))) {
    *aCanHandleContent = PR_TRUE;
  }
  else {
    *aCanHandleContent = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::CanHandleContent(const char        *aContentType,
				       nsURILoadCommand   aCommand,
				       char             **aDesiredContentType,
				       PRBool            *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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

