/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "libimg.h"
#include "nsImageNet.h"
#include "ilINetContext.h"
#include "ilIURL.h"
#include "ilINetReader.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsITimer.h"
#include "nsVoidArray.h"
#include "prmem.h"
#include "plstr.h"
#include "il_strm.h"

static NS_DEFINE_IID(kIImageNetContextIID, IL_INETCONTEXT_IID);
static NS_DEFINE_IID(kIURLIID, NS_IURL_IID);

class ImageNetContextImpl : public ilINetContext {
public:
  ImageNetContextImpl(NET_ReloadMethod aReloadPolicy);
  ~ImageNetContextImpl();

  NS_DECL_ISUPPORTS

  virtual ilINetContext* Clone();

  virtual NET_ReloadMethod GetReloadPolicy();

  virtual void AddReferer(ilIURL *aUrl);

  virtual void Interrupt();

  virtual ilIURL* CreateURL(const char *aUrl, 
			    NET_ReloadMethod aReloadMethod);

  virtual PRBool IsLocalFileURL(char *aAddress);

  virtual PRBool IsURLInMemCache(ilIURL *aUrl);

  virtual PRBool IsURLInDiskCache(ilIURL *aUrl);

  virtual int GetURL (ilIURL * aUrl, NET_ReloadMethod aLoadMethod,
		      ilINetReader *aReader);

  nsITimer *mTimer;
  nsVoidArray *mRequests;
  NET_ReloadMethod mReloadPolicy;
};

typedef struct {
    ilIURL *mURL;
    nsIInputStream *mStream;
} NetRequestStr;

ImageNetContextImpl::ImageNetContextImpl(NET_ReloadMethod aReloadPolicy)
{
  NS_INIT_REFCNT();
  mTimer = nsnull;
  mRequests = nsnull;
  mReloadPolicy = aReloadPolicy;
}

ImageNetContextImpl::~ImageNetContextImpl()
{
  if (mTimer != nsnull) {
      mTimer->Cancel();
      NS_RELEASE(mTimer);
  }

  if (mRequests != nsnull) {
      int i, count = mRequests->Count();
      for (i=0; i < count; i++) {
          NetRequestStr *req = (NetRequestStr *)mRequests->ElementAt(i);

          if (req->mURL != nsnull) {
              NS_RELEASE(req->mURL);
          }
          
          if (req->mStream != nsnull) {
              NS_RELEASE(req->mStream);
          }

          PR_DELETE(req);
      }
      delete mRequests;
  }
}

NS_IMPL_ISUPPORTS(ImageNetContextImpl, kIImageNetContextIID)

ilINetContext* 
ImageNetContextImpl::Clone()
{
    ilINetContext *cx;

    if (NS_NewImageNetContext(&cx) == NS_OK) {
        return cx;
    }
    else {
        return nsnull;
    }
}

NET_ReloadMethod 
ImageNetContextImpl::GetReloadPolicy()
{
    return mReloadPolicy;
}

void 
ImageNetContextImpl::AddReferer(ilIURL *aUrl)
{
}

void 
ImageNetContextImpl::Interrupt()
{
}

ilIURL* 
ImageNetContextImpl::CreateURL(const char *aURL, 
			       NET_ReloadMethod aReloadMethod)
{
    ilIURL *url;

    if (NS_NewImageURL(&url, aURL) == NS_OK) {
        return url;
    }
    else {
        return nsnull;
    }
}

PRBool 
ImageNetContextImpl::IsLocalFileURL(char *aAddress)
{
    if (PL_strncasecmp(aAddress, "file:", 5) == 0) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}

PRBool 
ImageNetContextImpl::IsURLInMemCache(ilIURL *aUrl)
{
    return PR_FALSE;
}

PRBool 
ImageNetContextImpl::IsURLInDiskCache(ilIURL *aUrl)
{
    return PR_TRUE;
}

#define IMAGE_BUF_SIZE 4096

void
LoadURLs (nsITimer *aTimer, void *aClosure)
{
    ImageNetContextImpl *cx = (ImageNetContextImpl *)aClosure;
    
    NS_RELEASE(aTimer);
    cx->mTimer = nsnull;

    if (cx->mRequests) {
        int i, count = cx->mRequests->Count();
    
        char *buffer = (char *)PR_MALLOC(IMAGE_BUF_SIZE);
        if (buffer == nsnull) {
            return;
        }
        
        for (i=0; i < count; i++) {
            NetRequestStr *req = (NetRequestStr *)cx->mRequests->ElementAt(i);
            ilIURL *ilurl = req->mURL;
            PRInt32 status = 0;
            PRBool reschedule = PR_FALSE;
            
            if (ilurl != nsnull) {
	            nsIURL *nsurl;
                ilINetReader *reader = ilurl->GetReader();
                nsIInputStream *stream = nsnull;
                
                if (req->mStream != nsnull) {
                    stream = req->mStream;
                }
                else {
                    if (reader->StreamCreated(ilurl, IL_UNKNOWN) == PR_TRUE &&
                        ilurl->QueryInterface(kIURLIID, (void **)&nsurl) == NS_OK) { 
                        PRInt32 ec;
                        stream = nsurl->Open(&ec);
                        if (nsnull == stream) {
                            reader->StreamAbort(-1);
                            status = -1;
                        }
                    }
                    else {
                        reader->StreamAbort(-1);
                        status = -1;
                    }
                }
             
                if (status == 0) {
                    PRInt32 err = 0, nb;
                    PRBool first = (PRBool)(req->mStream == nsnull);
                    do {
                       if (reader->WriteReady() <= 0) {
                           req->mStream = stream;
                           reschedule = PR_TRUE;
                           break;
                       }
                       err = 0;
                       nb = stream->Read(&err, buffer, 0, 
                                         IMAGE_BUF_SIZE);
                       if (err != 0 && err != NS_INPUTSTREAM_EOF) {
                           break;
                       }
                       if (first == PR_TRUE) {
                           PRInt32 ilErr;

                           ilErr = reader->FirstWrite((const unsigned char *)buffer, nb);
                           first = PR_FALSE;
                           /* 
                            * If FirstWrite(...) fails then the image type
                            * cannot be determined and the il_container 
                            * stream functions have not been initialized!
                            */
                           if (ilErr != 0) {
                               break;
                           }
                       }
                       
                       reader->Write((const unsigned char *)buffer, nb);
                    } while (err != NS_INPUTSTREAM_EOF);
                    
                    if (err == NS_INPUTSTREAM_EOF) {
                        reader->StreamComplete(PR_FALSE);
                    }
                    else if (err != 0) {
                        reader->StreamAbort(-1);
                        status = -1;
                    }
                }

                if (!reschedule) {
                    if (stream != nsnull) {
                        NS_RELEASE(stream);
                    }
                    req->mStream = nsnull;
                    reader->NetRequestDone(ilurl, status);
                    NS_RELEASE(reader);
                    NS_RELEASE(ilurl);
                }
            }
        }
	
        for (i=count-1; i >= 0; i--) {
            NetRequestStr *req = (NetRequestStr *)cx->mRequests->ElementAt(i);
            
            if (req->mStream == nsnull) {
                cx->mRequests->RemoveElementAt(i);
                PR_DELETE(req);
            }
            else if (cx->mTimer == nsnull) {
                if (NS_NewTimer(&cx->mTimer) == NS_OK) {
                    cx->mTimer->Init(LoadURLs, (void *)cx, 0);
                }
            }
        }
           
        PR_FREEIF(buffer);
    }
}

int 
ImageNetContextImpl::GetURL (ilIURL * aURL, 
			     NET_ReloadMethod aLoadMethod,
			     ilINetReader *aReader)
{
    NS_PRECONDITION(nsnull != aURL, "null URL");
    NS_PRECONDITION(nsnull != aReader, "null reader");
    if (aURL == nsnull || aReader == nsnull) {
        return -1;
    }
    
    if (mRequests == nsnull) {
        mRequests = new nsVoidArray();
        if (mRequests == nsnull) {
            // XXX Should still call exit function
            return -1;
        }
    }

    if (mTimer == nsnull) {
        if (NS_NewTimer(&mTimer) != NS_OK) {
            // XXX Should still call exit function
            return -1;
        }
        
        if (mTimer->Init(LoadURLs, (void *)this, 0) != NS_OK) {
            // XXX Should still call exit function
            return -1;
        }
    }

    aURL->SetReader(aReader);

    NetRequestStr *req = PR_NEWZAP(NetRequestStr);
    req->mURL = aURL;
    NS_ADDREF(aURL);

    mRequests->AppendElement((void *)req);

    return 0;
}

nsresult 
NS_NewImageNetContext(ilINetContext **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  
  ilINetContext *cx = new ImageNetContextImpl(NET_NORMAL_RELOAD);
  if (cx == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return cx->QueryInterface(kIImageNetContextIID, (void **) aInstancePtrResult);
}
