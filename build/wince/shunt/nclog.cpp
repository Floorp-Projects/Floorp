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
 * The Original Code is NCLog.
 *
 * The Initial Developer of the Original Code is Marco Manfredini.
 *
 * Code posted to USENET:
 *
 *     http://groups-beta.google.com/group/microsoft.public.windowsce.embedded/
 *            browse_thread/thread/352904e5ff1bfeb/
 *            9ad05ef17dda0203?q=outputdebugstring+wince#9ad05ef17dda0203
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Doug Turner <dougt@meer.net>
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


#include "mozce_internal.h"

#define USE_NC_LOGGING

#ifdef USE_NC_LOGGING


#include "winsock.h"
#include <stdarg.h>
#include <stdio.h>

static SOCKET wsa_socket=INVALID_SOCKET;
#pragma comment(lib , "winsock")

static unsigned short theLogPort;

// bind the log socket to a specific port.
static bool wsa_bind(unsigned short port)
{
  SOCKADDR_IN addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  int r=bind(wsa_socket,(sockaddr*)&addr,sizeof(addr));
  if (r==0) theLogPort=port;
  return (r==0);
  
}

// initialize everything, if the socket isn't open.
static bool wsa_init()
{
  if (wsa_socket != INVALID_SOCKET) return true;
  int r;
  WSADATA wd;
  BOOL bc=true;
  
  if (0 != WSAStartup(0x101, &wd)) 
  {
	  MessageBox(0, L"WSAStartup failed", L"ERROR", 0);
  	  goto error;
  }
  wsa_socket=socket(PF_INET, SOCK_DGRAM, 0);
  if (wsa_socket == INVALID_SOCKET)
  {
	  MessageBox(0, L"socket failed", L"ERROR", 0);
	  goto error;
  }
  r=setsockopt(wsa_socket, SOL_SOCKET, SO_BROADCAST, (char*)&bc,
               sizeof(bc));
  if (r!=0)
  {
	MessageBox(0, L"setsockopt failed", L"ERROR", 0);
	goto error;
  }

  if (wsa_bind(9998)) return true; // bind to default port.

  MessageBox(0, L"Can Not Bind To Port", L"ERROR", 0);

error:
  if (wsa_socket != INVALID_SOCKET) closesocket(wsa_socket);
  return false;

}

// can be called externally to select a different port for operations
bool set_nclog_port(unsigned short x) { return wsa_bind(x); }

static void wsa_send(const char *x)
{
  SOCKADDR_IN sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(theLogPort);
  sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  
  sendto(wsa_socket, x, strlen(x), 0, (sockaddr*)&sa, sizeof(sa));
}

// format input, convert to 8-bit and send.
int nclog (const char *fmt, ...)
{
  va_list vl;
  va_start(vl,fmt);
  char buf[1024]; // to bad CE hasn't got wvnsprintf
  sprintf(buf,fmt,vl);
  wsa_init();
  wsa_send(buf);
  return 0;
}

void nclograw(const char* data, long length)
{
  wsa_init();
  
  SOCKADDR_IN sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(theLogPort);
  sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  
  sendto(wsa_socket, data, length, 0, (sockaddr*)&sa, sizeof(sa));
}

// finalize the socket on program termination.
struct _nclog_module
{
  ~_nclog_module()
  {
    if (wsa_socket!=INVALID_SOCKET)
    {
      nclog("nclog goes down\n");
      shutdown(wsa_socket,2);
      closesocket(wsa_socket);
    }
  }
  
};

static _nclog_module module; 


#endif
