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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
 *
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

#using <mscorlib.dll>

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "umURI.h"
#include "DotNETNetworking.h"

using namespace System;
using namespace System::Runtime::InteropServices;

using namespace Mozilla::Embedding::Networking;

#pragma unmanaged
unmanagedURI::unmanagedURI()
{
  mUri = nsnull;
}

unmanagedURI::~unmanagedURI()
{
}

nsresult unmanagedURI::CreateURI()
{
  return NS_NewURI(getter_AddRefs(mUri), "");
}

nsresult unmanagedURI::CreateURI(PRUnichar* url)
{
  nsAutoString urlString;

  urlString.Assign(url);
  return NS_NewURI(getter_AddRefs(mUri), urlString);
}

const char* unmanagedURI::GetSpec()
{
  nsresult rv = NS_OK;
  nsCAutoString curSpec;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetSpec(curSpec);
  if (NS_FAILED(rv)) 
    return nsnull;

  return curSpec.get();
}

const char* unmanagedURI::GetPrePath()
{
  nsresult rv = NS_OK;
  nsCAutoString prePath;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetPrePath(prePath);
  if (NS_FAILED(rv)) 
    return nsnull;

  return prePath.get();
}

const char* unmanagedURI::GetScheme()
{
  nsresult rv = NS_OK;
  nsCAutoString scheme;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetScheme(scheme);
  if (NS_FAILED(rv)) 
    return nsnull;

  return scheme.get();
}

void unmanagedURI::SetScheme(char* aScheme)
{
  nsresult rv = NS_OK;
  nsCAutoString scheme(aScheme);

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return;

  mUri->SetScheme(scheme);
}

const char* unmanagedURI::GetUserPass()
{
  nsresult rv = NS_OK;
  nsCAutoString userPass;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetUserPass(userPass);
  if (NS_FAILED(rv)) 
    return nsnull;

  return userPass.get();
}

void unmanagedURI::SetUserPass(char* aUserPass)
{
  nsresult rv = NS_OK;
  nsCAutoString userPass(aUserPass);

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return;

  mUri->SetUserPass(userPass);
}

const char* unmanagedURI::GetUsername()
{
  nsresult rv = NS_OK;
  nsCAutoString username;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetUsername(username);
  if (NS_FAILED(rv)) 
    return nsnull;

  return username.get();
}

void unmanagedURI::SetUsername(char* aUsername)
{
  nsresult rv = NS_OK;
  nsCAutoString username(aUsername);

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return;

  mUri->SetUsername(username);
}

const char* unmanagedURI::GetPassword()
{
  nsresult rv = NS_OK;
  nsCAutoString password;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetPassword(password);
  if (NS_FAILED(rv)) 
    return nsnull;

  return password.get();
}

void unmanagedURI::SetPassword(char* aPassword)
{
  nsresult rv = NS_OK;
  nsCAutoString password(aPassword);

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return;

  mUri->SetPassword(password);
}

const char* unmanagedURI::GetHostPort()
{
  nsresult rv = NS_OK;
  nsCAutoString hostPort;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetHostPort(hostPort);
  if (NS_FAILED(rv)) 
    return nsnull;

  return hostPort.get();
}

void unmanagedURI::SetHostPort(char* aHostPort)
{
  nsresult rv = NS_OK;
  nsCAutoString hostPort(aHostPort);

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return;

  mUri->SetHostPort(hostPort);
}

const char* unmanagedURI::GetHost()
{
  nsresult rv = NS_OK;
  nsCAutoString host;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetHostPort(host);
  if (NS_FAILED(rv)) 
    return nsnull;

  return host.get();
}

void unmanagedURI::SetHost(char* aHost)
{
  nsresult rv = NS_OK;
  nsCAutoString host(aHost);

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return;

  mUri->SetHost(host);
}

PRInt32 unmanagedURI::GetPort()
{
  nsresult rv = NS_OK;
  PRInt32 port;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return 0;

  rv = mUri->GetPort(&port);
  if (NS_FAILED(rv)) 
    return 0;

  return port;
}

void unmanagedURI::SetPort(PRInt32 aPort)
{
  nsresult rv = NS_OK;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return;

  mUri->SetPort(aPort);
}

const char* unmanagedURI::GetPath()
{
  nsresult rv = NS_OK;
  nsCAutoString path;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetPath(path);
  if (NS_FAILED(rv)) 
    return nsnull;

  return path.get();
}

void unmanagedURI::SetPath(char* aPath)
{
  nsresult rv = NS_OK;
  nsCAutoString path(aPath);

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return;

  mUri->SetPath(path);
}

bool unmanagedURI::Equals(unmanagedURI* aOther)
{
  nsresult rv = NS_OK;
  PRBool equal = PR_FALSE;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return false;

  mUri->Equals(aOther->mUri, &equal);

  return (equal == PR_FALSE) ? false : true;
}

bool unmanagedURI::SchemeIs(char* aScheme)
{
  nsresult rv = NS_OK;
  PRBool isSheme = PR_FALSE;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return false;

  mUri->SchemeIs(aScheme, &isSheme);

  return (isSheme == PR_FALSE) ? false : true;
}

const char * unmanagedURI::Resolve(char* aRelativePath)
{
  nsresult rv = NS_OK;
  nsCAutoString relativePath(aRelativePath);
  nsCAutoString path;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->Resolve(relativePath, path);
  if (NS_FAILED(rv)) 
    return nsnull;

  return path.get();
}

const char* unmanagedURI::GetAsciiSpec()
{
  nsresult rv = NS_OK;
  nsCAutoString asciiSpec;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetAsciiSpec(asciiSpec);
  if (NS_FAILED(rv)) 
    return nsnull;

  return asciiSpec.get();
}


const char* unmanagedURI::GetAsciiHost()
{
  nsresult rv = NS_OK;
  nsCAutoString asciiHost;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetAsciiHost(asciiHost);
  if (NS_FAILED(rv)) 
    return nsnull;

  return asciiHost.get();
}

const char* unmanagedURI::GetOriginCharset()
{
  nsresult rv = NS_OK;
  nsCAutoString originalCharset;

  if (!mUri)
    rv = CreateURI();
  if (NS_FAILED(rv))
    return nsnull;

  rv = mUri->GetOriginCharset(originalCharset);
  if (NS_FAILED(rv)) 
    return nsnull;

  return originalCharset.get();
}

#pragma managed
URI::URI()
{
}

URI::~URI()
{
  Dispose(false);
}

void URI::Dispose()
{
  Dispose(true);
}

void URI::Dispose(bool disposing)
{
  if (mURI)   
  {
    delete mURI;
    mURI = NULL;
  }

  if (disposing)  
  {
    GC::SuppressFinalize(this);
  }
}
bool URI::CreateUnmanagedURI()
{
  bool ret = false;

  try 
  {
    mURI = new unmanagedURI();
    if (mURI)
      ret = true;
  }
  catch (Exception*) 
  {
    throw;
  }

  return ret;
}

String* URI::get_Spec() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((wchar_t *)mURI->GetSpec());
}

String* URI::get_PrePath() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((wchar_t *)mURI->GetPrePath());
}

String* URI::get_Scheme() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((wchar_t *)mURI->GetScheme());
}

void URI::set_Scheme(String *aScheme) 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return;
  }

  try 
  {
    IntPtr schemePtr = Marshal::StringToHGlobalAnsi(aScheme);
    char * pScheme = (char *)schemePtr.ToPointer();

    mURI->SetScheme(pScheme);

    Marshal::FreeHGlobal(pScheme);
  }
  catch (Exception*) 
  {
    throw;
  }
}

String* URI::get_UserPass() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringAnsi((char *)mURI->GetScheme());
}

void URI::set_UserPass(String *aUserPass) 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return;
  }

  try 
  {
    IntPtr userPassPtr = Marshal::StringToHGlobalAnsi(aUserPass);
    char * pUserPass = (char *)userPassPtr.ToPointer();

    mURI->SetUserPass(pUserPass);

    Marshal::FreeHGlobal(pUserPass);
  }
  catch (Exception*) 
  {
    throw;
  }
}

String* URI::get_Username() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringAnsi((char *)mURI->GetUsername());
}

void URI::set_Username(String *aUsername) 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return;
  }

  try 
  {
    IntPtr userNamePtr = Marshal::StringToHGlobalAnsi(aUsername);
    char * pUserName = (char *)userNamePtr.ToPointer();

    mURI->SetUsername(pUserName);

    Marshal::FreeHGlobal(pUserName);
  }
  catch (Exception*) 
  {
    throw;
  }
}

String* URI::get_Password() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((char *)mURI->GetPassword());
}

void URI::set_Password(String *aPassword) 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return;
  }

  try 
  {
    IntPtr passwordPtr = Marshal::StringToHGlobalAnsi(aPassword);
    char * pPassword = (char *)passwordPtr.ToPointer();

    mURI->SetPassword(pPassword);

    Marshal::FreeHGlobal(pPassword);
  }
  catch (Exception*) 
  {
    throw;
  }
}

String* URI::get_HostPort() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((char *)mURI->GetHostPort());
}

void URI::set_HostPort(String *aHostPort) 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return;
  }

  try 
  {
    IntPtr hostPortPtr = Marshal::StringToHGlobalAnsi(aHostPort);
    char * pHostPort = (char *)hostPortPtr.ToPointer();

    mURI->SetHostPort(pHostPort);

    Marshal::FreeHGlobal(pHostPort);
  }
  catch (Exception*) 
  {
    throw;
  } 
}

String* URI::get_Host() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((char *)mURI->GetHost());
}

void URI::set_Host(String *aHost) 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return;
  }

  try 
  {
    IntPtr hostPtr = Marshal::StringToHGlobalAnsi(aHost);
    char * pHost = (char *)hostPtr.ToPointer();

    mURI->SetHost(pHost);

    Marshal::FreeHGlobal(pHost);
  }
  catch (Exception*) 
  {
    throw;
  } 
}

IntPtr URI::get_Port() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return 0;
  }

  return mURI->GetPort();
}

void URI::set_Port(IntPtr aPort) 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return;
  }

  try 
  {
    mURI->SetPort(aPort.ToInt32());
  }
  catch (Exception*) 
  {
    throw;
  }
} 

String* URI::get_Path() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((char *)mURI->GetPath());
}

void URI::set_Path(String *aPath) 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return;
  }

  try 
  {
    IntPtr pathPtr = Marshal::StringToHGlobalAnsi(aPath);
    char * pPath = (char *)pathPtr.ToPointer();

    mURI->SetPath(pPath);

    Marshal::FreeHGlobal(pPath);
  }
  catch (Exception*) 
  {
    throw;
  }
}

bool URI::Equals(URI *aOther)
{
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return false;
  }

  return mURI->Equals(aOther->mURI);
}

bool URI::SchemeIs(String *aScheme)
{
  bool scheme = false;

  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return false;
  }

  try 
  {
    IntPtr schemePtr = Marshal::StringToHGlobalAnsi(aScheme);
    char * pScheme = (char *)schemePtr.ToPointer();

    scheme = mURI->SchemeIs(pScheme);

    Marshal::FreeHGlobal(pScheme);
  }
  catch (Exception*) 
  {
    throw;
  } 

  return scheme;
}

String* URI::Resolve(String *aRelativePath)
{
  String *resolvedPath;

  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  try 
  {
    IntPtr relativePathPtr = Marshal::StringToHGlobalAnsi(aRelativePath);
    char * pRelativePath = (char *)relativePathPtr.ToPointer();

    resolvedPath = mURI->Resolve(pRelativePath);

    Marshal::FreeHGlobal(pRelativePath);
  }
  catch (Exception*) 
  {
    throw;
  }

  return resolvedPath;
}

String* URI::get_AsciiSpec() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((char *)mURI->GetAsciiSpec());
}
 
String* URI::get_AsciiHost() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((char *)mURI->GetAsciiHost());
}

String* URI::get_OriginCharset() 
{ 
  if (!mURI) 
  {
    if (!CreateUnmanagedURI())  
      return NULL;
  }

  return Marshal::PtrToStringUni((char *)mURI->GetOriginCharset());
}


