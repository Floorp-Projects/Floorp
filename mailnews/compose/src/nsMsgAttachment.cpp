/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMsgAttachment.h"
#include "nsILocalFile.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS1(nsMsgAttachment, nsIMsgAttachment)

nsMsgAttachment::nsMsgAttachment()
{
  NS_INIT_ISUPPORTS();
  mTemporary = PR_FALSE;
}

nsMsgAttachment::~nsMsgAttachment()
{
  if (mTemporary)
    (void)DeleteAttachment();
}

/* attribute wstring name; */
NS_IMETHODIMP nsMsgAttachment::GetName(PRUnichar * *aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  *aName = ToNewUnicode(mName);
  return (*aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}
NS_IMETHODIMP nsMsgAttachment::SetName(const PRUnichar * aName)
{
  mName = aName;
  return NS_OK;
}

/* attribute string url; */
NS_IMETHODIMP nsMsgAttachment::GetUrl(char * *aUrl)
{
  NS_ENSURE_ARG_POINTER(aUrl);

  *aUrl = ToNewCString(mUrl);
  return (*aUrl ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}
NS_IMETHODIMP nsMsgAttachment::SetUrl(const char * aUrl)
{
  mUrl = aUrl;
  return NS_OK;
}

/* attribute boolean temporary; */
NS_IMETHODIMP nsMsgAttachment::GetTemporary(PRBool *aTemporary)
{
  NS_ENSURE_ARG_POINTER(aTemporary);

  *aTemporary = mTemporary;
  return NS_OK;
}
NS_IMETHODIMP nsMsgAttachment::SetTemporary(PRBool aTemporary)
{
  mTemporary = aTemporary;
  return NS_OK;
}

/* attribute string contentLocation; */
NS_IMETHODIMP nsMsgAttachment::GetContentLocation(char * *aContentLocation)
{
  NS_ENSURE_ARG_POINTER(aContentLocation);

  *aContentLocation = ToNewCString(mContentLocation);
  return (*aContentLocation ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}
NS_IMETHODIMP nsMsgAttachment::SetContentLocation(const char * aContentLocation)
{
  mContentLocation = aContentLocation;
  return NS_OK;
}

/* attribute string contentType; */
NS_IMETHODIMP nsMsgAttachment::GetContentType(char * *aContentType)
{
  NS_ENSURE_ARG_POINTER(aContentType);

  *aContentType = ToNewCString(mContentType);
  return (*aContentType ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}
NS_IMETHODIMP nsMsgAttachment::SetContentType(const char * aContentType)
{
  mContentType = aContentType;
  /* a full content type could also contains parameters but we need to
     keep only the content type alone. Therefore we need to cleanup it.
  */
  PRInt32 offset = mContentType.FindChar(';');
  if (offset)
    mContentType.Truncate(offset);

  return NS_OK;
}

/* attribute string macType; */
NS_IMETHODIMP nsMsgAttachment::GetMacType(char * *aMacType)
{
  NS_ENSURE_ARG_POINTER(aMacType);

  *aMacType = ToNewCString(mMacType);
  return (*aMacType ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}
NS_IMETHODIMP nsMsgAttachment::SetMacType(const char * aMacType)
{
  mMacType = aMacType;
  return NS_OK;
}

/* attribute string macCreator; */
NS_IMETHODIMP nsMsgAttachment::GetMacCreator(char * *aMacCreator)
{
  NS_ENSURE_ARG_POINTER(aMacCreator);

  *aMacCreator = ToNewCString(mMacCreator);
  return (*aMacCreator ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}
NS_IMETHODIMP nsMsgAttachment::SetMacCreator(const char * aMacCreator)
{
  mMacCreator = aMacCreator;
  return NS_OK;
}

/* boolean equalsUrl (in nsIMsgAttachment attachment); */
NS_IMETHODIMP nsMsgAttachment::EqualsUrl(nsIMsgAttachment *attachment, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(attachment);
  NS_ENSURE_ARG_POINTER(_retval);

  nsXPIDLCString url;
  attachment->GetUrl(getter_Copies(url));

  *_retval = mUrl.Equals(url);
  return NS_OK;
}


nsresult nsMsgAttachment::DeleteAttachment()
{
	nsresult rv;
  PRBool isAFile = PR_FALSE;

  nsCOMPtr<nsILocalFile> urlFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv))
	{
    rv = urlFile->SetURL(mUrl);
    if (NS_SUCCEEDED(rv))
	  {
      PRBool bExists = PR_FALSE;
      rv = urlFile->Exists(&bExists);
	    if (NS_FAILED(rv)) 
        {NS_ASSERTION(0, "bExists() call failed!");}
      else
        if (bExists)
        {
          rv = urlFile->IsFile(&isAFile);
          if (NS_FAILED(rv)) 
            {NS_ASSERTION(0, "IsFile() call failed!");}
        }
    }
    else
      {NS_ASSERTION(0, "Can't set url in nsILocalFile interface");}
  }
  else
    {NS_ASSERTION(0, "Can't creat nsIFileURL interface");}


  // remove it if it's a valid file
  if (isAFile)
	  rv = urlFile->Remove(PR_FALSE); 

  return rv;
}
