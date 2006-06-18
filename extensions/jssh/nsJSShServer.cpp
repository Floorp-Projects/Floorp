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
 * The Original Code is Mozilla JavaScript Shell project.
 *
 * The Initial Developer of the Original Code is
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com>
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

#include "nsJSShServer.h"
#include "nsNetCID.h"
#include "nsISocketTransport.h"
#include "nsIComponentManager.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsThreadUtils.h"
#include "nsJSSh.h"

//**********************************************************************
// ConnectionListener helper class

class ConnectionListener : public nsIServerSocketListener
{
public:
  ConnectionListener(const nsACString &startupURI);
  ~ConnectionListener();
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVERSOCKETLISTENER

private:
  nsCString mStartupURI;
};

//----------------------------------------------------------------------
// Implementation

ConnectionListener::ConnectionListener(const nsACString &startupURI)
    : mStartupURI(startupURI)
{
}

ConnectionListener::~ConnectionListener()
{ 

}

already_AddRefed<nsIServerSocketListener> CreateConnectionListener(const nsACString &startupURI)
{
  nsIServerSocketListener* obj = new ConnectionListener(startupURI);
  NS_IF_ADDREF(obj);
  return obj;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_THREADSAFE_ADDREF(ConnectionListener)
NS_IMPL_THREADSAFE_RELEASE(ConnectionListener)

NS_INTERFACE_MAP_BEGIN(ConnectionListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIServerSocketListener)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsIServerSocketListener methods:

/* void onSocketAccepted (in nsIServerSocket aServ, in nsISocketTransport aTransport); */
NS_IMETHODIMP ConnectionListener::OnSocketAccepted(nsIServerSocket *aServ, nsISocketTransport *aTransport)
{
  nsCOMPtr<nsIInputStream> input;
  nsCOMPtr<nsIOutputStream> output;
  aTransport->OpenInputStream(nsITransport::OPEN_BLOCKING, 0, 0, getter_AddRefs(input));
  aTransport->OpenOutputStream(nsITransport::OPEN_BLOCKING, 0, 0, getter_AddRefs(output));
  
#ifdef DEBUG
  printf("JSSh server: new connection!\n");
#endif

  nsCOMPtr<nsIRunnable> shell = CreateJSSh(input, output, mStartupURI);

  nsCOMPtr<nsIThread> thread;
  NS_NewThread(getter_AddRefs(thread), shell);
  return NS_OK;
}

/* void onStopListening (in nsIServerSocket aServ, in nsresult aStatus); */
NS_IMETHODIMP ConnectionListener::OnStopListening(nsIServerSocket *aServ, nsresult aStatus)
{
#ifdef DEBUG
  printf("JSSh server: stopped listening!\n");
#endif
  return NS_OK;
}

//**********************************************************************
// nsJSShServer implementation

nsJSShServer::nsJSShServer()
{
#ifdef DEBUG
  printf("nsJSShServer ctor\n");
#endif
}

nsJSShServer::~nsJSShServer()
{
#ifdef DEBUG
  printf("nsJSShServer dtor\n");
#endif
  // XXX should we stop listening or not??
  StopServerSocket();
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsJSShServer)
NS_IMPL_RELEASE(nsJSShServer)

NS_INTERFACE_MAP_BEGIN(nsJSShServer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIJSShServer)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsIJSShServer methods:

/* void startServerSocket (in unsigned long port, in AUTF8String startupURI,
   in boolean loopbackOnly); */
NS_IMETHODIMP
nsJSShServer::StartServerSocket(PRUint32 port, const nsACString & startupURI,
                                PRBool loopbackOnly)
{
  if (mServerSocket) {
    NS_ERROR("server socket already running");
    return NS_ERROR_FAILURE;
  }
  
  mServerSocket = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID);
  if (!mServerSocket) {
    NS_ERROR("could not create server socket");
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(mServerSocket->Init(port, loopbackOnly, -1))) {
    mServerSocket = nsnull;
    NS_ERROR("server socket initialization error");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIServerSocketListener> listener = CreateConnectionListener(startupURI);
  mServerSocket->AsyncListen(listener);

  mServerPort = port;
  mServerStartupURI = startupURI;
  mServerLoopbackOnly = loopbackOnly;
    
  return NS_OK;
}

/* void stopServerSocket (); */
NS_IMETHODIMP nsJSShServer::StopServerSocket()
{
  if (mServerSocket)
    mServerSocket->Close();
  mServerSocket = nsnull;
  mServerPort = 0;
  mServerStartupURI.Truncate();
  mServerLoopbackOnly = PR_FALSE;
  return NS_OK;
}

/* void runShell (in nsIInputStream input, in nsIOutputStream output,
                  in string startupURI, in boolean blocking); */
NS_IMETHODIMP
nsJSShServer::RunShell(nsIInputStream *input, nsIOutputStream *output,
                       const char *startupURI, PRBool blocking)
{
  nsCOMPtr<nsIRunnable> shell = CreateJSSh(input, output, nsCString(startupURI));
  if (!blocking) {
    nsCOMPtr<nsIThread> thread;
    NS_NewThread(getter_AddRefs(thread), shell);
  }
  else
    shell->Run();

  return NS_OK;
}

/* readonly attribute boolean serverListening; */
NS_IMETHODIMP
nsJSShServer::GetServerListening(PRBool *aServerListening)
{
  if (mServerSocket)
    *aServerListening = PR_TRUE;
  else
    *aServerListening = PR_FALSE;
  
  return NS_OK;
}

/* readonly attribute unsigned long serverPort; */
NS_IMETHODIMP
nsJSShServer::GetServerPort(PRUint32 *aServerPort)
{
  *aServerPort = mServerPort;
  return NS_OK;
}

/* readonly attribute AUTF8String serverStartupURI; */
NS_IMETHODIMP
nsJSShServer::GetServerStartupURI(nsACString & aServerStartupURI)
{
  aServerStartupURI = mServerStartupURI;
  return NS_OK;
}

/* readonly attribute boolean serverLoopbackOnly; */
NS_IMETHODIMP
nsJSShServer::GetServerLoopbackOnly(PRBool *aServerLoopbackOnly)
{
  *aServerLoopbackOnly = mServerLoopbackOnly;
  return NS_OK;
}
