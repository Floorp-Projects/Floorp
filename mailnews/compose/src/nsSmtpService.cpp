/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"    // precompiled header...
#include "nsXPIDLString.h"
#include "nsIPref.h"
#include "nsIIOService.h"

#include "nsSmtpService.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"

#include "nsSmtpUrl.h"
#include "nsSmtpProtocol.h"
#include "nsIFileSpec.h"
#include "nsCOMPtr.h"
#include "nsIMsgIdentity.h"
#include "nsIPrompt.h"
#include "nsMsgComposeStringBundle.h"

typedef struct _findServerByKeyEntry {
    const char *key;
    nsISmtpServer *server;
} findServerByKeyEntry;

typedef struct _findServerByHostnameEntry {
    const char *hostname;
    const char *username;
    nsISmtpServer *server;
} findServerByHostnameEntry;

static NS_DEFINE_CID(kCSmtpUrlCID, NS_SMTPURL_CID);
static NS_DEFINE_CID(kCMailtoUrlCID, NS_MAILTOURL_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID); 
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID); 

// foward declarations...
nsresult
NS_MsgBuildSmtpUrl(nsIFileSpec * aFilePath,
                   const char* aSmtpHostName, 
                   const char* aSmtpUserName, 
                   const char* aRecipients, 
                   nsIMsgIdentity * aSenderIdentity,
                   nsIUrlListener * aUrlListener,
                   nsIPrompt * aNetPrompt,
                   nsIURI ** aUrl);

nsresult NS_MsgLoadSmtpUrl(nsIURI * aUrl, nsISupports * aConsumer);

nsSmtpService::nsSmtpService() :
    mSmtpServersLoaded(PR_FALSE)
{
    NS_INIT_REFCNT();
    NS_NewISupportsArray(getter_AddRefs(mSmtpServers));
}

nsSmtpService::~nsSmtpService()
{
    // save the SMTP servers to disk

}

NS_IMPL_ISUPPORTS2(nsSmtpService, nsISmtpService, nsIProtocolHandler);


nsresult nsSmtpService::SendMailMessage(nsIFileSpec * aFilePath,
                                        const char * aRecipients, 
                                        nsIMsgIdentity * aSenderIdentity,
                                        nsIUrlListener * aUrlListener, 
                                        nsISmtpServer * aServer,
                                        nsIPrompt * aNetPrompt,
                                        nsIURI ** aURL)
{
	nsIURI * urlToRun = nsnull;
	nsresult rv = NS_OK;

  nsCOMPtr<nsISmtpServer> smtpServer;

  // first try the identity's preferred server
  if (aSenderIdentity) {
      nsXPIDLCString smtpServerKey;
      rv = aSenderIdentity->GetSmtpServerKey(getter_Copies(smtpServerKey));
      if (NS_SUCCEEDED(rv) && (const char *)smtpServerKey)
          rv = GetServerByKey(smtpServerKey,
                                           getter_AddRefs(smtpServer));
  }

  // fallback to the default
  if (NS_FAILED(rv) || !smtpServer)
      rv = GetDefaultServer(getter_AddRefs(smtpServer));

	if (NS_SUCCEEDED(rv) && smtpServer)
	{
    nsXPIDLCString smtpHostName;
    nsXPIDLCString smtpUserName;

		smtpServer->GetHostname(getter_Copies(smtpHostName));
		smtpServer->GetUsername(getter_Copies(smtpUserName));

    if ((const char*)smtpHostName && (const char*)smtpHostName[0] != 0) 
		{
      rv = NS_MsgBuildSmtpUrl(aFilePath, smtpHostName, smtpUserName,
                              aRecipients, aSenderIdentity, aUrlListener,
                              aNetPrompt, &urlToRun); // this ref counts urlToRun
      if (NS_SUCCEEDED(rv) && urlToRun)	
      {
        nsCOMPtr<nsISmtpUrl> smtpUrl = do_QueryInterface(urlToRun, &rv);
        if (NS_SUCCEEDED(rv))
            smtpUrl->SetSmtpServer(smtpServer);
        rv = NS_MsgLoadSmtpUrl(urlToRun, nsnull);
      }

      if (aURL) // does the caller want a handle on the url?
        *aURL = urlToRun; // transfer our ref count to the caller....
      else
        NS_IF_RELEASE(urlToRun);
    }
    else
      rv = NS_ERROR_COULD_NOT_LOGIN_TO_SMTP_SERVER;
	}

	return rv;
}


// The following are two convience functions I'm using to help expedite building and running a mail to url...

// short cut function for creating a mailto url...
nsresult NS_MsgBuildSmtpUrl(nsIFileSpec * aFilePath,
                            const char* aSmtpHostName, 
                            const char* aSmtpUserName, 
                            const char * aRecipients, 
                            nsIMsgIdentity * aSenderIdentity,
                            nsIUrlListener * aUrlListener, 
                            nsIPrompt * aNetPrompt,
                            nsIURI ** aUrl)
{
	// mscott: this function is a convience hack until netlib actually dispatches smtp urls.
	// in addition until we have a session to get a password, host and other stuff from, we need to use default values....
	// ..for testing purposes....
	
	nsresult rv = NS_OK;
	nsCOMPtr <nsISmtpUrl> smtpUrl (do_CreateInstance(kCSmtpUrlCID, &rv));

	if (NS_SUCCEEDED(rv) && smtpUrl)
	{
		nsCAutoString urlSpec("smtp://");
		if ((const char *)aSmtpUserName) {
			nsXPIDLCString escapedUsername;
			*((char **)getter_Copies(escapedUsername)) = nsEscape((const char *)aSmtpUserName, url_XAlphas);
			urlSpec += (const char *)escapedUsername;
			urlSpec += '@';
		}

		urlSpec += (const char*)aSmtpHostName;
		urlSpec += ':';
		urlSpec.AppendInt(SMTP_PORT);

		if ((const char *)urlSpec)
		{
			nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(smtpUrl);
			url->SetSpec((const char *)urlSpec);
            smtpUrl->SetRecipients(aRecipients);
			smtpUrl->SetPostMessageFile(aFilePath);
			smtpUrl->SetSenderIdentity(aSenderIdentity);
            smtpUrl->SetPrompt(aNetPrompt);
			url->RegisterListener(aUrlListener);
		}
		rv = smtpUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) aUrl);
	 }

	 return rv;
}

nsresult NS_MsgLoadSmtpUrl(nsIURI * aUrl, nsISupports * aConsumer)
{
	// for now, assume the url is a news url and load it....
	nsCOMPtr <nsISmtpUrl> smtpUrl;
	nsSmtpProtocol	*smtpProtocol = nsnull;
	nsresult rv = NS_OK;

	if (!aUrl)
		return rv;

    // turn the url into an smtp url...
	smtpUrl = do_QueryInterface(aUrl);
  if (smtpUrl)
  {
		// almost there...now create a nntp protocol instance to run the url in...
		smtpProtocol = new nsSmtpProtocol(aUrl);
		if (smtpProtocol == nsnull)
			rv = NS_ERROR_OUT_OF_MEMORY;
		else
			rv = smtpProtocol->LoadUrl(aUrl, aConsumer); // protocol will get destroyed when url is completed...
	}

	return rv;
}

NS_IMETHODIMP nsSmtpService::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
		*aScheme = PL_strdup("mailto");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsSmtpService::GetDefaultPort(PRInt32 *aDefaultPort)
{
	nsresult rv = NS_OK;
	if (aDefaultPort)
		*aDefaultPort = SMTP_PORT;
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 	
}

//////////////////////////////////////////////////////////////////////////
// This is just a little stub channel class for mailto urls. Mailto urls
// don't really have any data for the stream calls in nsIChannel to make much sense.
// But we need to have a channel to return for nsSmtpService::NewChannel
// that can simulate a real channel such that the uri loader can then get the
// content type for the channel.
class nsMailtoChannel : public nsIChannel
{
public:

	  NS_DECL_ISUPPORTS
    NS_DECL_NSICHANNEL
    NS_DECL_NSIREQUEST
	
    nsMailtoChannel(nsIURI * aURI);
	  virtual ~nsMailtoChannel();

protected:
  nsCOMPtr<nsIURI> m_url;
  nsresult mStatus;
};

nsMailtoChannel::nsMailtoChannel(nsIURI * aURI)
    : m_url(aURI), mStatus(NS_OK)
{
  NS_INIT_ISUPPORTS();
}

nsMailtoChannel::~nsMailtoChannel()
{}

NS_IMPL_ISUPPORTS2(nsMailtoChannel, nsIChannel, nsIRequest);

NS_IMETHODIMP nsMailtoChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsMailtoChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
	return NS_OK;
}

NS_IMETHODIMP nsMailtoChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    NS_NOTREACHED("GetNotificationCallbacks");
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
	return NS_OK;       // don't fail when trying to set this
}

NS_IMETHODIMP 
nsMailtoChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsMailtoChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = nsnull;
    return NS_OK; 
}
 
NS_IMETHODIMP nsMailtoChannel::SetOriginalURI(nsIURI* aURI)
{
  return NS_OK;
}
 
NS_IMETHODIMP nsMailtoChannel::GetURI(nsIURI* *aURI)
{
  *aURI = m_url;
  NS_IF_ADDREF(*aURI);
  return NS_OK; 
}
 
NS_IMETHODIMP nsMailtoChannel::SetURI(nsIURI* aURI)
{
  m_url = aURI;
  return NS_OK; 
}
 
NS_IMETHODIMP nsMailtoChannel::OpenInputStream(nsIInputStream **_retval)
{
  NS_NOTREACHED("OpenInputStream");
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::OpenOutputStream(nsIOutputStream **_retval)
{
  NS_NOTREACHED("OpenOutputStream");
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::AsyncRead(nsIStreamListener *listener, nsISupports *ctxt)
{
  return listener->OnStartRequest(this, ctxt);
}

NS_IMETHODIMP nsMailtoChannel::AsyncWrite(nsIInputStream *fromStream, nsIStreamObserver *observer, nsISupports *ctxt)
{
    NS_NOTREACHED("AsyncWrite");
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::GetContentType(char * *aContentType)
{
	*aContentType = nsCRT::strdup("x-application-mailto");
	return NS_OK;
}

NS_IMETHODIMP nsMailtoChannel::SetContentType(const char *aContentType)
{
    // Do not allow the content type to change...
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMailtoChannel::GetContentLength(PRInt32 * aContentLength)
{
  *aContentLength = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsMailtoChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
    NS_NOTREACHED("GetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
    NS_NOTREACHED("SetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::GetTransferCount(PRInt32 *aTransferCount)
{
    NS_NOTREACHED("GetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::SetTransferCount(PRInt32 aTransferCount)
{
    NS_NOTREACHED("SetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
    NS_NOTREACHED("GetBufferSegmentSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
    NS_NOTREACHED("SetBufferSegmentSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
    NS_NOTREACHED("GetBufferMaxSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
    NS_NOTREACHED("SetBufferMaxSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::GetLocalFile(nsIFile* *file)
{
    NS_NOTREACHED("GetLocalFile");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMailtoChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
nsMailtoChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::GetOwner(nsISupports * *aPrincipal)
{
    NS_NOTREACHED("GetOwner");
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::SetOwner(nsISupports * aPrincipal)
{
    NS_NOTREACHED("SetOwner");
	return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMailtoChannel::GetName(PRUnichar* *result)
{
    NS_NOTREACHED("nsMailtoChannel::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::IsPending(PRBool *result)
{
    *result = PR_TRUE;
    return NS_OK; 
}

NS_IMETHODIMP nsMailtoChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP nsMailtoChannel::Cancel(nsresult status)
{
    mStatus = status;
    return NS_OK;
}

NS_IMETHODIMP nsMailtoChannel::Suspend()
{
    NS_NOTREACHED("Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMailtoChannel::Resume()
{
    NS_NOTREACHED("Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
}


// the smtp service is also the protocol handler for mailto urls....

NS_IMETHODIMP nsSmtpService::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
  // get a new smtp url 

  nsresult rv = NS_OK;
	nsCOMPtr <nsIURI> mailtoUrl;

	rv = nsComponentManager::CreateInstance(kCMailtoUrlCID, NULL, NS_GET_IID(nsIURI), getter_AddRefs(mailtoUrl));

	if (NS_SUCCEEDED(rv))
	{
    mailtoUrl->SetSpec(aSpec);
		rv = mailtoUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) _retval);
	}
  return rv;
}

NS_IMETHODIMP nsSmtpService::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  nsresult rv = NS_OK;
  nsMailtoChannel * aMailtoChannel = new nsMailtoChannel(aURI);
  if (aMailtoChannel)
  {
      rv = aMailtoChannel->QueryInterface(NS_GET_IID(nsIChannel), (void **) _retval);
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;
  return rv;
}


NS_IMETHODIMP
nsSmtpService::GetSmtpServers(nsISupportsArray ** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsresult rv;
  
  // now read in the servers from prefs if necessary
  PRUint32 serverCount;
  rv = mSmtpServers->Count(&serverCount);
  if (NS_FAILED(rv)) return rv;

  if (serverCount<=0) loadSmtpServers();

  *aResult = mSmtpServers;
  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult
nsSmtpService::loadSmtpServers()
{
    if (mSmtpServersLoaded) return NS_OK;
    
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsXPIDLCString serverList;
    rv = prefs->CopyCharPref("mail.smtpservers", getter_Copies(serverList));
    if (NS_SUCCEEDED(rv)) {

        char *newStr;
        char *pref = nsCRT::strtok(NS_CONST_CAST(char*,(const char*)serverList),
                                   ", ", &newStr);
        while (pref) {
            
            rv = createKeyedServer(pref);
            
            pref = nsCRT::strtok(newStr, ", ", &newStr);
        }

    }

    saveKeyList();

    mSmtpServersLoaded = PR_TRUE;
    return NS_OK;
}

// save the list of keys
nsresult
nsSmtpService::saveKeyList()
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    return prefs->SetCharPref("mail.smtpservers", mServerKeyList);
}

nsresult
nsSmtpService::createKeyedServer(const char *key, nsISmtpServer** aResult)
{
    if (!key) return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsISmtpServer> server;
    
    nsresult rv;
    rv = nsComponentManager::CreateInstance(NS_SMTPSERVER_CONTRACTID,
                                            nsnull,
                                            NS_GET_IID(nsISmtpServer),
                                            (void **)getter_AddRefs(server));
    if (NS_FAILED(rv)) return rv;
    
    server->SetKey(NS_CONST_CAST(char *,key));
    mSmtpServers->AppendElement(server);

    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        if (mServerKeyList.IsEmpty())
            mServerKeyList = key;
        else {
            mServerKeyList += ",";
            mServerKeyList += key;
        }
    }

    if (aResult) {
        *aResult = server;
        NS_IF_ADDREF(*aResult);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpService::GetSessionDefaultServer(nsISmtpServer **aServer)
{
    NS_ENSURE_ARG_POINTER(aServer);
    
    if (!mSessionDefaultServer)
        return GetDefaultServer(aServer);

    *aServer = mSessionDefaultServer;
    NS_ADDREF(*aServer);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpService::SetSessionDefaultServer(nsISmtpServer *aServer)
{
    mSessionDefaultServer = aServer;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpService::GetDefaultServer(nsISmtpServer **aServer)
{
  NS_ENSURE_ARG_POINTER(aServer);

  nsresult rv;

  loadSmtpServers();
  
  *aServer = nsnull;
  // always returns NS_OK, just leaving *aServer at nsnull
  if (!mDefaultSmtpServer) {
      NS_WITH_SERVICE(nsIPref, pref, NS_PREF_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return rv;

      // try to get it from the prefs
      nsXPIDLCString defaultServerKey;
      rv = pref->CopyCharPref("mail.smtp.defaultserver",
                             getter_Copies(defaultServerKey));
      if (NS_SUCCEEDED(rv) &&
          nsCRT::strlen(defaultServerKey) > 0) {

          nsCOMPtr<nsISmtpServer> server;
          rv = GetServerByKey(defaultServerKey,
                              getter_AddRefs(mDefaultSmtpServer));
      } else {
          // no pref set, so just return the first one, and set the pref
      
          PRUint32 count=0;
          nsCOMPtr<nsISupportsArray> smtpServers;
          rv = GetSmtpServers(getter_AddRefs(smtpServers));
          rv = smtpServers->Count(&count);

          // nothing in the array, we had better create a new server
          // (which will add it to the array & prefs anyway)
          if (count == 0)
              rv = CreateSmtpServer(getter_AddRefs(mDefaultSmtpServer));
          else
              rv = mSmtpServers->QueryElementAt(0, NS_GET_IID(nsISmtpServer),
                                                (void **)getter_AddRefs(mDefaultSmtpServer));

          if (NS_FAILED(rv)) return rv;
          NS_ENSURE_TRUE(mDefaultSmtpServer, NS_ERROR_UNEXPECTED);
          
          // now we have a default server, set the prefs correctly
          nsXPIDLCString serverKey;
          mDefaultSmtpServer->GetKey(getter_Copies(serverKey));
          if (NS_SUCCEEDED(rv))
              pref->SetCharPref("mail.smtp.defaultserver", serverKey);
      }
  }

  // at this point:
  // * mDefaultSmtpServer has a valid server
  // * the key has been set in the prefs
    
  *aServer = mDefaultSmtpServer;
  NS_IF_ADDREF(*aServer);

  return NS_OK;
}

NS_IMETHODIMP
nsSmtpService::SetDefaultServer(nsISmtpServer *aServer)
{
    nsresult rv;
    mDefaultSmtpServer = aServer;

    nsXPIDLCString serverKey;
    rv = aServer->GetKey(getter_Copies(serverKey));
    if (NS_FAILED(rv)) return rv;
    
    NS_WITH_SERVICE(nsIPref, pref, NS_PREF_CONTRACTID, &rv);
    pref->SetCharPref("mail.smtp.defaultserver", serverKey);
    return NS_OK;
}

PRBool
nsSmtpService::findServerByKey (nsISupports *element, void *aData)
{
    nsresult rv;
    nsCOMPtr<nsISmtpServer> server = do_QueryInterface(element, &rv);
    if (NS_FAILED(rv)) return PR_TRUE;
    
    findServerByKeyEntry *entry = (findServerByKeyEntry*) aData;

    nsXPIDLCString key;
    rv = server->GetKey(getter_Copies(key));
    if (NS_FAILED(rv)) return PR_TRUE;

    if (nsCRT::strcmp(key, entry->key)==0) {
        entry->server = server;
        return PR_FALSE;
    }
    
    return PR_TRUE;
}

NS_IMETHODIMP
nsSmtpService::CreateSmtpServer(nsISmtpServer **aResult)
{
    if (!aResult) return NS_ERROR_NULL_POINTER;

    loadSmtpServers();
    nsresult rv;
    
    PRInt32 i=0;
    PRBool unique = PR_FALSE;

    findServerByKeyEntry entry;
    nsCAutoString key;
    
    do {
        key = "smtp";
        key.AppendInt(++i);
        
        entry.key = key;
        entry.server = nsnull;

        mSmtpServers->EnumerateForwards(findServerByKey, (void *)&entry);
        if (!entry.server) unique=PR_TRUE;
        
    } while (!unique);

    rv = createKeyedServer(key, aResult);
    saveKeyList();
    return rv;
}


nsresult
nsSmtpService::GetServerByKey(const char* aKey, nsISmtpServer **aResult)
{
    findServerByKeyEntry entry;
    entry.key = aKey;
    entry.server = nsnull;
    mSmtpServers->EnumerateForwards(findServerByKey, (void *)&entry);

    if (entry.server) {
        (*aResult) = entry.server;
        NS_ADDREF(*aResult);
        return NS_OK;
    }

    // not found in array, I guess we load it
    return createKeyedServer(aKey, aResult);
}

NS_IMETHODIMP
nsSmtpService::DeleteSmtpServer(nsISmtpServer *aServer)
{
    if (!aServer) return NS_OK;

    nsresult rv;

    PRInt32 idx = 0;
    rv = mSmtpServers->GetIndexOf(aServer, &idx);
    if (NS_FAILED(rv) || idx==-1)
        return NS_OK;

    nsXPIDLCString serverKey;
    aServer->GetKey(getter_Copies(serverKey));
    
    rv = mSmtpServers->DeleteElementAt(idx);

    if (mDefaultSmtpServer.get() == aServer)
        mDefaultSmtpServer = nsnull;
    if (mSessionDefaultServer.get() == aServer)
        mSessionDefaultServer = nsnull;
    
    nsCAutoString newServerList;
    char *newStr;
    char *rest = mServerKeyList.ToNewCString();
    
    char *token = nsCRT::strtok(rest, ",", &newStr);
    while (token) {
        // only re-add the string if it's not the key
        if (nsCRT::strcmp(token, serverKey) != 0) {
            if (newServerList.IsEmpty())
                newServerList = token;
            else {
                newServerList += ',';
                newServerList += token;
            }
        }

        token = nsCRT::strtok(newStr, ",", &newStr);
    }

    mServerKeyList = newServerList;
    saveKeyList();
    return rv;
}

PRBool
nsSmtpService::findServerByHostname(nsISupports *element, void *aData)
{
    nsresult rv;
    
    nsCOMPtr<nsISmtpServer> server = do_QueryInterface(element, &rv);
    if (NS_FAILED(rv)) return PR_TRUE;

    findServerByHostnameEntry *entry = (findServerByHostnameEntry*)aData;

    nsXPIDLCString hostname;
    rv = server->GetHostname(getter_Copies(hostname));
    if (NS_FAILED(rv)) return PR_TRUE;

    nsXPIDLCString username;
    rv = server->GetUsername(getter_Copies(username));
    if (NS_FAILED(rv)) return PR_TRUE;

    PRBool checkHostname = entry->hostname && PL_strcmp(entry->hostname, "");
    PRBool checkUsername = entry->username && PL_strcmp(entry->username, "");
    
    if ((!checkHostname || (PL_strcasecmp(entry->hostname, hostname)==0)) &&
        (!checkUsername || (PL_strcmp(entry->username, username)==0))) {
        entry->server = server;
        return PR_FALSE;        // stop when found
    }
    return PR_TRUE;
}

NS_IMETHODIMP
nsSmtpService::FindServer(const char *aUsername,
                          const char *aHostname, nsISmtpServer ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    findServerByHostnameEntry entry;
    entry.server=nsnull;
    entry.hostname = aHostname;
    entry.username = aUsername;

    mSmtpServers->EnumerateForwards(findServerByHostname, (void *)&entry);

    // entry.server may be null, but that's ok.
    // just return null if no server is found
    *aResult = entry.server;
    NS_IF_ADDREF(*aResult);
    
    return NS_OK;
}
