/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "msgCore.h"    // precompiled header...
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIPref.h"
#include "nsIIOService.h"
#include "nsIPipe.h"
#include "nsNetCID.h"
#include "nsEscape.h"
#include "nsNetUtil.h"

#include "nsSmtpService.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"

#include "nsSmtpUrl.h"
#include "nsSmtpProtocol.h"
#include "nsIFileSpec.h"
#include "nsCOMPtr.h"
#include "nsIMsgIdentity.h"
#include "nsMsgComposeStringBundle.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsMsgSimulateError.h"
#include "nsIUTF8ConverterService.h"
#include "nsUConvCID.h"

#define SERVER_DELIMITER ","
#define APPEND_SERVERS_VERSION_PREF_NAME "append_preconfig_smtpservers.version"
#define MAIL_ROOT_PREF "mail."
#define PREF_MAIL_SMTPSERVERS "mail.smtpservers"
#define PREF_MAIL_SMTPSERVERS_APPEND_SERVERS "mail.smtpservers.appendsmtpservers"

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

// foward declarations...
nsresult
NS_MsgBuildSmtpUrl(nsIFileSpec * aFilePath,
                   const char* aSmtpHostName, 
                   PRInt32 aSmtpPort,
                   const char* aSmtpUserName, 
                   const char* aRecipients, 
                   nsIMsgIdentity * aSenderIdentity,
                   nsIUrlListener * aUrlListener,
                   nsIMsgStatusFeedback *aStatusFeedback,
                   nsIInterfaceRequestor* aNotificationCallbacks,
                   nsIURI ** aUrl);

nsresult NS_MsgLoadSmtpUrl(nsIURI * aUrl, nsISupports * aConsumer, nsIRequest ** aRequest);

nsSmtpService::nsSmtpService() :
    mSmtpServersLoaded(PR_FALSE)
{
    NS_NewISupportsArray(getter_AddRefs(mSmtpServers));
}

nsSmtpService::~nsSmtpService()
{
    // save the SMTP servers to disk

}

NS_IMPL_ISUPPORTS2(nsSmtpService, nsISmtpService, nsIProtocolHandler)


nsresult nsSmtpService::SendMailMessage(nsIFileSpec * aFilePath,
                                        const char * aRecipients, 
                                        nsIMsgIdentity * aSenderIdentity,
                                        const char * aPassword,
                                        nsIUrlListener * aUrlListener, 
                                        nsIMsgStatusFeedback *aStatusFeedback,
                                        nsIInterfaceRequestor* aNotificationCallbacks,
                                        nsIURI ** aURL,
                                        nsIRequest ** aRequest)
{
  nsIURI * urlToRun = nsnull;
  nsresult rv = NS_OK;

  nsCOMPtr<nsISmtpServer> smtpServer;
  rv = GetSmtpServerByIdentity(aSenderIdentity, getter_AddRefs(smtpServer));

  if (NS_SUCCEEDED(rv) && smtpServer)
  {
    if (aPassword && *aPassword)
      smtpServer->SetPassword(aPassword);

    nsXPIDLCString smtpHostName;
    nsXPIDLCString smtpUserName;
    PRInt32 smtpPort;
    PRInt32 trySSL;

    smtpServer->GetHostname(getter_Copies(smtpHostName));
    smtpServer->GetUsername(getter_Copies(smtpUserName));
    smtpServer->GetPort(&smtpPort);
    smtpServer->GetTrySSL(&trySSL);

    if (smtpPort == 0)
    {
        if (trySSL == PREF_SECURE_ALWAYS_SMTPS)
            smtpPort = nsISmtpUrl::DEFAULT_SMTPS_PORT;
        else
            smtpPort = nsISmtpUrl::DEFAULT_SMTP_PORT;
    }

    if (smtpHostName && smtpHostName.get()[0] && !CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_10)) 
    {
      rv = NS_MsgBuildSmtpUrl(aFilePath, smtpHostName, smtpPort, smtpUserName,
                              aRecipients, aSenderIdentity, aUrlListener, aStatusFeedback, 
                              aNotificationCallbacks, &urlToRun); // this ref counts urlToRun
      if (NS_SUCCEEDED(rv) && urlToRun)	
      {
        nsCOMPtr<nsISmtpUrl> smtpUrl = do_QueryInterface(urlToRun, &rv);
        if (NS_SUCCEEDED(rv))
            smtpUrl->SetSmtpServer(smtpServer);
        rv = NS_MsgLoadSmtpUrl(urlToRun, nsnull, aRequest);
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
                            PRInt32 aSmtpPort,
                            const char* aSmtpUserName, 
                            const char * aRecipients, 
                            nsIMsgIdentity * aSenderIdentity,
                            nsIUrlListener * aUrlListener, 
                            nsIMsgStatusFeedback *aStatusFeedback,
                            nsIInterfaceRequestor* aNotificationCallbacks,
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
        if (aSmtpUserName) 
        {
            nsXPIDLCString escapedUsername;
            *((char **)getter_Copies(escapedUsername)) = nsEscape(aSmtpUserName, url_XAlphas);
            urlSpec += escapedUsername;
            urlSpec += '@';
        }

        urlSpec += aSmtpHostName;
        if (!PL_strchr(aSmtpHostName, ':'))
        {
            urlSpec += ':';
            urlSpec.AppendInt(aSmtpPort);
        }

        if (urlSpec.get())
        {
            nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(smtpUrl);
            url->SetSpec(urlSpec);
            smtpUrl->SetRecipients(aRecipients);
            smtpUrl->SetPostMessageFile(aFilePath);
            smtpUrl->SetSenderIdentity(aSenderIdentity);
            smtpUrl->SetNotificationCallbacks(aNotificationCallbacks);

            nsCOMPtr<nsIPrompt> smtpPrompt(do_GetInterface(aNotificationCallbacks));
            nsCOMPtr<nsIAuthPrompt> smtpAuthPrompt(do_GetInterface(aNotificationCallbacks));
            if (!smtpPrompt || !smtpAuthPrompt)
            {
                nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
                if (wwatch) {
                    if (!smtpPrompt)
                        wwatch->GetNewPrompter(0, getter_AddRefs(smtpPrompt));
                    if (!smtpAuthPrompt)
                        wwatch->GetNewAuthPrompter(0, getter_AddRefs(smtpAuthPrompt));
                }
            }
            smtpUrl->SetPrompt(smtpPrompt);            
            smtpUrl->SetAuthPrompt(smtpAuthPrompt);
            url->RegisterListener(aUrlListener);
            if (aStatusFeedback)
                url->SetStatusFeedback(aStatusFeedback);
        }
        rv = smtpUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) aUrl);
    }

    return rv;
}

nsresult NS_MsgLoadSmtpUrl(nsIURI * aUrl, nsISupports * aConsumer, nsIRequest ** aRequest)
{
    // for now, assume the url is an smtp url and load it....
    nsCOMPtr <nsISmtpUrl> smtpUrl;
    nsSmtpProtocol	*smtpProtocol = nsnull;
    nsresult rv = NS_OK;

    if (!aUrl)
        return rv;

    // turn the url into an smtp url...
    smtpUrl = do_QueryInterface(aUrl);
    if (smtpUrl)
    {
        // almost there...now create a smtp protocol instance to run the url in...
        smtpProtocol = new nsSmtpProtocol(aUrl);
        if (smtpProtocol == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(smtpProtocol);
        rv = smtpProtocol->LoadUrl(aUrl, aConsumer); // protocol will get destroyed when url is completed...
        smtpProtocol->QueryInterface(NS_GET_IID(nsIRequest), (void **) aRequest);
        NS_RELEASE(smtpProtocol);
    }

    return rv;
}

NS_IMETHODIMP nsSmtpService::GetScheme(nsACString &aScheme)
{
    aScheme = "mailto";
    return NS_OK; 
}

NS_IMETHODIMP nsSmtpService::GetDefaultPort(PRInt32 *aDefaultPort)
{
    nsresult rv = NS_OK;
    if (aDefaultPort)
        *aDefaultPort = nsISmtpUrl::DEFAULT_SMTP_PORT;
    else
        rv = NS_ERROR_NULL_POINTER;
    return rv;
}

NS_IMETHODIMP 
nsSmtpService::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // allow smtp to run on any port
    *_retval = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsSmtpService::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_NORELATIVE | ALLOWS_PROXY;
    return NS_OK; 	
}

// the smtp service is also the protocol handler for mailto urls....

NS_IMETHODIMP nsSmtpService::NewURI(const nsACString &aSpec,
                                    const char *aOriginCharset,
                                    nsIURI *aBaseURI,
                                    nsIURI **_retval)
{
  // get a new smtp url 

  nsresult rv;
  nsCOMPtr <nsIURI> mailtoUrl = do_CreateInstance(kCMailtoUrlCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCAutoString utf8Spec;
    if (aOriginCharset)
    {
       nsCOMPtr<nsIUTF8ConverterService> 
           utf8Converter(do_GetService(NS_UTF8CONVERTERSERVICE_CONTRACTID, &rv));
       if (NS_SUCCEEDED(rv))
          rv = utf8Converter->ConvertURISpecToUTF8(aSpec, aOriginCharset, utf8Spec);
    }

    // utf8Spec is filled up only when aOriginCharset is specified and 
    // the conversion is successful. Otherwise, fall back to aSpec.
    if (aOriginCharset && NS_SUCCEEDED(rv))
      mailtoUrl->SetSpec(utf8Spec);
    else
      mailtoUrl->SetSpec(aSpec);
    rv = mailtoUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) _retval);
  }
  return rv;
}

NS_IMETHODIMP nsSmtpService::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  // create an empty pipe for use with the input stream channel.
  nsCOMPtr<nsIInputStream> pipeIn;
  nsCOMPtr<nsIOutputStream> pipeOut;
  nsresult rv = NS_NewPipe(getter_AddRefs(pipeIn),
                           getter_AddRefs(pipeOut));
  if (NS_FAILED(rv)) return rv;

  pipeOut->Close();

  return NS_NewInputStreamChannel(_retval, aURI, pipeIn,
                                  NS_LITERAL_CSTRING("x-application-mailto"));
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
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString tempServerList;
    nsXPIDLCString serverList;
    rv = prefs->CopyCharPref(PREF_MAIL_SMTPSERVERS, getter_Copies(tempServerList));

    //Get the pref in a tempServerList and then parse it to see if it has dupes.
    //if so remove the dupes and then create the serverList.
    if (!tempServerList.IsEmpty()) {

      // Tokenize the data and add each smtp server if it is not already there 
      // in the user's current smtp server list
      char *tempSmtpServerStr;
      char *tempSmtpServersStr = nsCRT::strdup(tempServerList.get());
      char *tempToken = nsCRT::strtok(tempSmtpServersStr, SERVER_DELIMITER, &tempSmtpServerStr);

      nsCAutoString tempSmtpServer;
      while (tempToken) {
        if (*tempToken) {
          if (serverList.IsEmpty() || !strstr(serverList.get(), tempToken)) {
            tempSmtpServer.Assign(tempToken);
            tempSmtpServer.StripWhitespace();
            if (!serverList.IsEmpty())
              serverList += SERVER_DELIMITER;
            serverList += tempSmtpServer;
          }
        }
        tempToken = nsCRT::strtok(tempSmtpServerStr, SERVER_DELIMITER, &tempSmtpServerStr);
      }
      nsCRT::free(tempSmtpServersStr);
    }
    else {
      serverList = tempServerList;
    }
      
    // We need to check if we have any pre-configured smtp servers so that
    // those servers can be appended to the list. 
    nsXPIDLCString appendServerList;
    rv = prefs->CopyCharPref(PREF_MAIL_SMTPSERVERS_APPEND_SERVERS,
					    getter_Copies(appendServerList));

    // Get the list of smtp servers (either from regular pref i.e, mail.smtpservers or
    // from preconfigured pref mail.smtpservers.appendsmtpservers) and create a keyed 
    // server list.
    if (!serverList.IsEmpty() || !appendServerList.IsEmpty()) {
      /** 
       * Check to see if we need to add pre-configured smtp servers.
       * Following prefs are important to note in understanding the procedure here.
       *
       * 1. pref("mailnews.append_preconfig_smtpservers.version", version number);
       * This pref registers the current version in the user prefs file. A default value 
       * is stored in mailnews.js file. If a given vendor needs to add more preconfigured 
       * smtp servers, the default version number can be increased. Comparing version 
       * number from user's prefs file and the default one from mailnews.js, we
       * can add new smp servers and any other version level changes that need to be done.
       *
       * 2. pref("mail.smtpservers.appendsmtpservers", <comma separated servers list>);
       * This pref contains the list of pre-configured smp servers that ISP/Vendor wants to
       * to add to the existing servers list. 
       */
      nsCOMPtr<nsIPrefBranch> defaultsPrefBranch;
      rv = prefs->GetDefaultBranch(MAIL_ROOT_PREF, getter_AddRefs(defaultsPrefBranch));
      NS_ENSURE_SUCCESS(rv,rv);

      nsCOMPtr<nsIPrefBranch> prefBranch;
      rv = prefs->GetBranch(MAIL_ROOT_PREF, getter_AddRefs(prefBranch));
      NS_ENSURE_SUCCESS(rv,rv);

      PRInt32 appendSmtpServersCurrentVersion=0;
      PRInt32 appendSmtpServersDefaultVersion=0;
      rv = prefBranch->GetIntPref(APPEND_SERVERS_VERSION_PREF_NAME, &appendSmtpServersCurrentVersion);
      NS_ENSURE_SUCCESS(rv,rv);

      rv = defaultsPrefBranch->GetIntPref(APPEND_SERVERS_VERSION_PREF_NAME, &appendSmtpServersDefaultVersion);
      NS_ENSURE_SUCCESS(rv,rv);

      // Update the smtp server list if needed
      if ((appendSmtpServersCurrentVersion <= appendSmtpServersDefaultVersion)) {
        // If there are pre-configured servers, add them to the existing server list
        if (!appendServerList.IsEmpty()) {
          if (!serverList.IsEmpty()) {
            nsCStringArray existingSmtpServersArray;
            existingSmtpServersArray.ParseString(serverList.get(), SERVER_DELIMITER);

            // Tokenize the data and add each smtp server if it is not already there 
            // in the user's current smtp server list
            char *newSmtpServerStr;
            char *preConfigSmtpServersStr = ToNewCString(appendServerList);
  
            char *token = nsCRT::strtok(preConfigSmtpServersStr, SERVER_DELIMITER, &newSmtpServerStr);

            nsCAutoString newSmtpServer;
            while (token) {
              if (token && *token) {
                newSmtpServer.Assign(token);
                newSmtpServer.StripWhitespace();

                if (existingSmtpServersArray.IndexOf(newSmtpServer) == -1) {
                  serverList += ",";
                  serverList += newSmtpServer;
                }
              }
              token = nsCRT::strtok(newSmtpServerStr, SERVER_DELIMITER, &newSmtpServerStr);
            }
            PR_Free(preConfigSmtpServersStr);
          }
          else {
            serverList = appendServerList;
          }
          // Increase the version number so that updates will happen as and when needed
          rv = prefBranch->SetIntPref(APPEND_SERVERS_VERSION_PREF_NAME, appendSmtpServersCurrentVersion + 1);
        }
      }

      char *newStr;
      char *pref = nsCRT::strtok(serverList.BeginWriting(), ", ", &newStr);

      while (pref) {
        // fix for bug #96207
        // code above makes sure that no duplicate entries in mail.smtpservers find
        // their way to the mSmtpServers list.  But it doesn't check, if a server to be
        // added already is in mSmtpServers.  That can happen in mail has been sent before 
        // opening the settings (loading the list).
        // use GetServerByKey to check if the key (pref) is already in
        // in the list. If not it calls createKeyedServer directly.
        nsCOMPtr<nsISmtpServer> server;
        rv = GetServerByKey(pref, getter_AddRefs(server));
        NS_ASSERTION(NS_SUCCEEDED(rv), "GetServerByKey failed");
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
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    
    return prefs->SetCharPref(PREF_MAIL_SMTPSERVERS, mServerKeyList.get());
}

nsresult
nsSmtpService::createKeyedServer(const char *key, nsISmtpServer** aResult)
{
    if (!key) return NS_ERROR_NULL_POINTER;
    
    nsresult rv;
    nsCOMPtr<nsISmtpServer> server = do_CreateInstance(NS_SMTPSERVER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    server->SetKey(key);
    mSmtpServers->AppendElement(server);

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
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
      nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID, &rv));
      if (NS_FAILED(rv)) return rv;

      // try to get it from the prefs
      nsXPIDLCString defaultServerKey;
      rv = pref->CopyCharPref("mail.smtp.defaultserver",
                             getter_Copies(defaultServerKey));
      if (NS_SUCCEEDED(rv) &&
          !defaultServerKey.IsEmpty()) {

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
              return nsnull;//if there are no smtp servers then dont create one for the default.
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
    NS_ENSURE_ARG_POINTER(aServer);

    mDefaultSmtpServer = aServer;

    nsXPIDLCString serverKey;
    nsresult rv = aServer->GetKey(getter_Copies(serverKey));
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID, &rv));
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
        
        entry.key = key.get();
        entry.server = nsnull;

        mSmtpServers->EnumerateForwards(findServerByKey, (void *)&entry);
        if (!entry.server) unique=PR_TRUE;
        
    } while (!unique);

    rv = createKeyedServer(key.get(), aResult);
    saveKeyList();
    return rv;
}


nsresult
nsSmtpService::GetServerByKey(const char* aKey, nsISmtpServer **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

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
    char *rest = ToNewCString(mServerKeyList);
    
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

    // make sure the server clears out it's values....
    aServer->ClearAllValues();

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

NS_IMETHODIMP
nsSmtpService::GetSmtpServerByIdentity(nsIMsgIdentity *aSenderIdentity, nsISmtpServer **aSmtpServer)
{
  NS_ENSURE_ARG_POINTER(aSmtpServer);
  nsresult rv = NS_ERROR_FAILURE;

  // First try the identity's preferred server
  if (aSenderIdentity) {
      nsXPIDLCString smtpServerKey;
      rv = aSenderIdentity->GetSmtpServerKey(getter_Copies(smtpServerKey));
      if (NS_SUCCEEDED(rv) && !(smtpServerKey.IsEmpty()))
          rv = GetServerByKey(smtpServerKey, aSmtpServer);
  }

  // Fallback to the default
  if (NS_FAILED(rv) || !(*aSmtpServer))
      rv = GetDefaultServer(aSmtpServer);
  return rv;
}
