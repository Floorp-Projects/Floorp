/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "stdafx.h"

#include "urlecho.h"

#include "wfedde.h"

extern "C" void FE_URLEcho(URL_Struct *pURL, int iStatus, MWContext *pContext)  {
//  Purpose:  Echo the URL to all appropriately registered applications that are monitoring such URL traffic.
//  Arguments:  pURL  The URL which is being loaded.
//        iStatus The status of the load.
//        pContext  The context in which the load occurred.
//  Returns:  void
//  Comments: Well, sometimes there isn't a window in which the load occurred, what to do then?
//        Just don't report.
//  Revision History:
//    01-18-95  created GAB
//

  //  Check to make sure everything's OK to continue onwards.
  if(pURL == NULL)  {
    return;
  }
  else if(pURL->address == NULL)  {
    return;
  } 
  else if(pContext == NULL) {
    return;
  }
  else if(GetFrame(pContext) == NULL) {
    //  Have to have a frame, as the DDE implementation assumes there are windows around.
    return;
  }
  
  //    Let the progress applications know what's up.
  if(pURL->server_status / 100 != 2 && pURL->server_status / 100 != 3 && pURL->server_status != 0) {
    //  The server actually reported an error, be we won't display this as an error but render it in a window.
    //  We will send these messages to an external type of applciation in this case only though that is monitoring
    //    progress.
    
    CString myMsg = szLoadString(IDS_SERVER_ERR_NUM);
    char aBuffer[16];
    sprintf(aBuffer, "%d", pURL->server_status);
    myMsg += aBuffer;
    return;
  }

  //  Okay, let's start checking stuff out.
  //  We will NOT report on failures to load.
  if(iStatus != MK_DATA_LOADED) {
    //  This is an error.
    //  Perhaps just an interrupting load by the user.
    //  In any event, it will be reported to any monitoring applications in WFE_Alert.
    return;
  }

  //  So the data was actually loaded.
  //  Build a URL, and a Referrer URL.
  //  Build the mime type, and a windowID.
  CString csURL;
  CString csReferrer;
  CString csMimeType;
  DWORD dwWindowID;
  
  csURL = pURL->address;
  if(pURL->referer != NULL) {
    csReferrer = pURL->referer;
  }
  if(pURL->content_type != NULL)  {
    csMimeType = pURL->content_type;
  }
  dwWindowID = FE_GetContextID(pContext);
  
  CEchoItem::Echo(csURL, csMimeType, dwWindowID, csReferrer);
  return;
} 

//  Static property.
CPtrList CEchoRegistry::m_Registry;

void CEchoItem::Echo(CString& csURL, CString& csMimeType, DWORD dwWindowID, CString& csReferrer)  {
//  Purpose:  Go through every object in the registry, and echo the URL information to them.
//  Arguments:  csURL The url being loaded.
//        csMimeType  The mime type of the URL being loaded.
//        dwWindowID  The window performing the load.
//        csReferrer  The referring URL.
//  Returns:  void
//  Comments:
//  Revision History:
//    01-18-95  created GAB
//

  //  Go through the registry.
  CEchoItem *pItem;
  POSITION rIndex = m_Registry.GetHeadPosition();
  while(rIndex != NULL) {
    pItem = (CEchoItem *)m_Registry.GetNext(rIndex);
    
    //  Depending on how the watching server was registered, it is handled through this virtual
    //    member mapping to the correct class.
    pItem->EchoURL(csURL, csMimeType, dwWindowID, csReferrer);
  }
}

void CDDEEchoItem::DDERegister(CString &csServiceName)  {
//  Purpose:  Register a DDE applciation to watch url echos.
//  Arguments:  csServiceName The service name of the DDE server which wants to watch the urls.
//  Returns:  void
//  Comments: Always works.
//  Revision History:
//    01-18-95  created GAB
//

  //  Just create the item.  Our job's done.
  CDDEEchoItem *pDontCare = new CDDEEchoItem(csServiceName);
}

BOOL CDDEEchoItem::DDEUnRegister(CString &csServiceName)  {
//  Purpose:  Deregister a DDE applciation watching url echos.
//  Arguments:  csServiceName The DDE service name to unregister.
//  Returns:  BOOL  Wether or not unregistered.
//  Comments: Have to look through every entry in the registry, until we find the entry.
//  Revision History:
//    01-18-95  created GAB
//

  //  Go through all registrations.
  POSITION rIndex = m_Registry.GetHeadPosition();
  CEchoItem *pItem;
  CDDEEchoItem *pDelMe;
  
  while(rIndex != NULL) {
    pItem = (CEchoItem *)m_Registry.GetNext(rIndex);
    
    //  Check the type of the registered app, could be OLE or something.
    if(pItem->GetType() == m_DDE) {
      pDelMe = (CDDEEchoItem *)pItem;
        if(pDelMe->GetServiceName() == csServiceName)  {
            delete pDelMe;
            return(TRUE);
        }
    }
  }

  return(FALSE);
}

void CDDEEchoItem::EchoURL(CString& csURL, CString& csMimeType, DWORD dwWindowID, CString& csReferrer)  {
//  Purpose:  Send a registered DDE server an URL echo event.
//  Arguments:  csURL The url loaded.
//        csMimeType  The mime type of the loading URL.
//        dwWindowID  The window performing the load.
//        csReferrer  The referrer URL.
//  Returns:  void
//  Comments: 
//  Revision History:
//    01-18-95  created GAB
//

  CDDEWrapper::URLEcho(this, csURL, csMimeType, dwWindowID, csReferrer);
}
