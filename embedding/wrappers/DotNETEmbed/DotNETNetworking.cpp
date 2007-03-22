/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
 *   Johnny Stenback <jst@netscape.com>
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

#using <mscorlib.dll>

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "DotNETNetworking.h"

using namespace Mozilla::Embedding::Networking;

URI::URI(nsIURI *aURI)
  : mURI(aURI)
{
  NS_IF_ADDREF(mURI);
}

URI::URI(String *aSpec)
  : mURI(nsnull)
{
  nsCAutoString spec;
  CopyString(aSpec, spec);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), spec.get());
  ThrowIfFailed(rv);

  mURI = uri.get();
  NS_IF_ADDREF(mURI);
}

URI::~URI()
{
  NS_IF_RELEASE(mURI);
}

Object *
URI::Clone()
{
  nsCOMPtr<nsIURI> uriClone;

  if (mURI) {
    nsresult rv = mURI->Clone(getter_AddRefs(uriClone));
    ThrowIfFailed(rv);
  }

  return new URI(uriClone);
}

void
URI::Dispose()
{
  NS_IF_RELEASE(mURI);

  GC::SuppressFinalize(this);
}

String *
URI::get_Spec()
{
  nsCAutoString spec;
  nsresult rv = mURI->GetSpec(spec);
  ThrowIfFailed(rv);

  return CopyString(spec);
}

void
URI::set_Spec(String *aSpec)
{
  nsCAutoString spec;
  CopyString(aSpec, spec);

  nsresult rv = mURI->SetSpec(spec);
  ThrowIfFailed(rv);
}

String *
URI::get_PrePath()
{
  nsCAutoString prePath;
  nsresult rv = mURI->GetPrePath(prePath);
  ThrowIfFailed(rv);

  return CopyString(prePath);
}

String *
URI::get_Scheme()
{
  nsCAutoString scheme;
  nsresult rv = mURI->GetScheme(scheme);
  ThrowIfFailed(rv);

  return CopyString(scheme);
}

void
URI::set_Scheme(String *aScheme)
{
  nsCAutoString scheme;
  CopyString(aScheme, scheme);

  nsresult rv = mURI->SetScheme(scheme);
  ThrowIfFailed(rv);
}

String *
URI::get_UserPass()
{
  nsCAutoString userPass;
  nsresult rv = mURI->GetUserPass(userPass);
  ThrowIfFailed(rv);

  return CopyString(userPass);
}

void
URI::set_UserPass(String *aUserPass)
{
  nsCAutoString userPass;
  CopyString(aUserPass, userPass);

  nsresult rv = mURI->SetUserPass(userPass);
  ThrowIfFailed(rv);
}

String *
URI::get_Username()
{
  nsCAutoString username;
  nsresult rv = mURI->GetUsername(username);
  ThrowIfFailed(rv);

  return CopyString(username);
}

void
URI::set_Username(String *aUsername)
{
  nsCAutoString username;
  CopyString(aUsername, username);

  nsresult rv = mURI->SetUsername(username);
  ThrowIfFailed(rv);
}

String *
URI::get_Password()
{
  nsCAutoString password;
  nsresult rv = mURI->GetPassword(password);
  ThrowIfFailed(rv);

  return CopyString(password);
}

void
URI::set_Password(String *aPassword)
{
  nsCAutoString password;
  CopyString(aPassword, password);

  nsresult rv = mURI->SetPassword(password);
  ThrowIfFailed(rv);
}

String *
URI::get_HostPort()
{
  nsCAutoString hostPort;
  nsresult rv = mURI->GetHostPort(hostPort);
  ThrowIfFailed(rv);

  return CopyString(hostPort);
}

void
URI::set_HostPort(String *aHostPort)
{
  nsCAutoString hostPort;
  CopyString(aHostPort, hostPort);

  nsresult rv = mURI->SetHostPort(hostPort);
  ThrowIfFailed(rv);
}

String *
URI::get_Host()
{
  nsCAutoString host;
  nsresult rv = mURI->GetHost(host);
  ThrowIfFailed(rv);

  return CopyString(host);
}

void
URI::set_Host(String *aHost)
{
  nsCAutoString host;
  CopyString(aHost, host);

  nsresult rv = mURI->SetHost(host);
  ThrowIfFailed(rv);
}

Int32
URI::get_Port()
{
  PRInt32 port;
  nsresult rv = mURI->GetPort(&port);
  ThrowIfFailed(rv);

  return port;
}

void
URI::set_Port(Int32 aPort)
{
  nsresult rv = mURI->SetPort(aPort);
  ThrowIfFailed(rv);
}

String *
URI::get_Path()
{
  nsCAutoString path;
  nsresult rv = mURI->GetPath(path);
  ThrowIfFailed(rv);

  return CopyString(path);
}

void
URI::set_Path(String *aPath)
{
  nsCAutoString path;
  CopyString(aPath, path);

  nsresult rv = mURI->SetPath(path);
  ThrowIfFailed(rv);
}

bool
URI::Equals(URI *aOther)
{
  if (!mURI) {
    if (!aOther || !aOther->mURI) {
      return true;
    }

    return false;
  }

  PRBool equals;
  nsresult rv = mURI->Equals(aOther->mURI, &equals);
  ThrowIfFailed(rv);

  return equals;
}

bool
URI::SchemeIs(String *aScheme)
{
  nsCAutoString scheme;
  CopyString(aScheme, scheme);

  PRBool isScheme = PR_FALSE;

  nsresult rv = mURI->SchemeIs(scheme.get(), &isScheme);
  ThrowIfFailed(rv);

  return isScheme;
}

String *
URI::Resolve(String *aRelativePath)
{
  nsCAutoString relativePath, resolvedPath;
  CopyString(aRelativePath, relativePath);

  nsresult rv = mURI->Resolve(relativePath, resolvedPath);
  ThrowIfFailed(rv);

  return CopyString(resolvedPath);
}

String *
URI::get_AsciiSpec()
{
  nsCAutoString asciiSpec;
  nsresult rv = mURI->GetAsciiSpec(asciiSpec);
  ThrowIfFailed(rv);

  return CopyString(asciiSpec);
}

String *
URI::get_AsciiHost()
{
  nsCAutoString asciiHost;
  nsresult rv = mURI->GetAsciiHost(asciiHost);
  ThrowIfFailed(rv);

  return CopyString(asciiHost);
}

String *
URI::get_OriginCharset()
{
  nsCAutoString originalCharset;
  nsresult rv = mURI->GetOriginCharset(originalCharset);
  ThrowIfFailed(rv);

  return CopyString(originalCharset);
}
