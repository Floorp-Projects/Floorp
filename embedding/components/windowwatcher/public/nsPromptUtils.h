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
 * The Original Code is mozilla.org embedding code.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Christian Biesinger <cbiesinger@web.de> (Original Author)
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

#ifndef NSPROMPTUTILS_H_
#define NSPROMPTUTILS_H_

#include "nsIHttpChannel.h"

/**
 * @file
 * This file defines some helper functions that simplify interaction
 * with authentication prompts.
 */

/**
 * Given a username (possibly in DOMAIN\user form) and password, parses the
 * domain out of the username if necessary and sets domain, username and
 * password on the auth information object.
 */
inline void
NS_SetAuthInfo(nsIAuthInformation* aAuthInfo, const nsString& user,
               const nsString& password)
{
  PRUint32 flags;
  aAuthInfo->GetFlags(&flags);
  if (flags & nsIAuthInformation::NEED_DOMAIN) {
    // Domain is separated from username by a backslash
    PRInt32 idx = user.FindChar(PRUnichar('\\'));
    if (idx == kNotFound) {
      aAuthInfo->SetUsername(user);
    } else {
      aAuthInfo->SetDomain(Substring(user, 0, idx));
      aAuthInfo->SetUsername(Substring(user, idx + 1));
    }
  } else {
    aAuthInfo->SetUsername(user);
  }
  aAuthInfo->SetPassword(password);
}

/**
 * Gets the host and port from a channel and authentication info. This is the
 * "logical" host and port for this authentication, i.e. for a proxy
 * authentication it refers to the proxy, while for a host authentication it
 * is the actual host.
 *
 * @param machineProcessing
 *        When this parameter is true, the host will be returned in ASCII
 *        (instead of UTF-8; this is relevant when IDN is used). In addition,
 *        the port will be returned as the real port even when it was not
 *        explicitly specified (when false, the port will be returned as -1 in
 *        this case)
 */
inline void
NS_GetAuthHostPort(nsIChannel* aChannel, nsIAuthInformation* aAuthInfo,
                   bool machineProcessing, nsCString& host, PRInt32* port)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv))
    return;

  // Have to distinguish proxy auth and host auth here...
  PRUint32 flags;
  aAuthInfo->GetFlags(&flags);
  if (flags & nsIAuthInformation::AUTH_PROXY) {
    nsCOMPtr<nsIProxiedChannel> proxied(do_QueryInterface(aChannel));
    NS_ASSERTION(proxied, "proxy auth needs nsIProxiedChannel");

    nsCOMPtr<nsIProxyInfo> info;
    proxied->GetProxyInfo(getter_AddRefs(info));
    NS_ASSERTION(info, "proxy auth needs nsIProxyInfo");

    nsCAutoString idnhost;
    info->GetHost(idnhost);
    info->GetPort(port);

    if (machineProcessing) {
      nsCOMPtr<nsIIDNService> idnService =
        do_GetService(NS_IDNSERVICE_CONTRACTID);
      if (idnService) {
        idnService->ConvertUTF8toACE(idnhost, host);
      } else {
        // Not much we can do here...
        host = idnhost;
      }
    } else {
      host = idnhost;
    }
  } else {
    if (machineProcessing) {
      uri->GetAsciiHost(host);
      *port = NS_GetRealPort(uri);
    } else {
      uri->GetHost(host);
      uri->GetPort(port);
    }
  }
}

/**
 * Creates the key for looking up passwords in the password manager. This
 * function uses the same format that Gecko functions have always used, thus
 * ensuring backwards compatibility.
 */
inline void
NS_GetAuthKey(nsIChannel* aChannel, nsIAuthInformation* aAuthInfo,
              nsCString& key)
{
  // HTTP does this differently from other protocols
  nsCOMPtr<nsIHttpChannel> http(do_QueryInterface(aChannel));
  if (!http) {
    nsCOMPtr<nsIURI> uri;
    aChannel->GetURI(getter_AddRefs(uri));
    uri->GetPrePath(key);
    return;
  }

  // NOTE: For backwards-compatibility reasons, this must be the ASCII host.
  nsCString host;
  PRInt32 port = -1;

  NS_GetAuthHostPort(aChannel, aAuthInfo, PR_TRUE, host, &port);

  nsAutoString realm;
  aAuthInfo->GetRealm(realm);
  
  // Now assemble the key: host:port (realm)
  key.Append(host);
  key.Append(':');
  key.AppendInt(port);
  key.AppendLiteral(" (");
  AppendUTF16toUTF8(realm, key);
  key.Append(')');
}

#endif

