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

#include "regproto.h"

#include "wfedde.h"
#include "oleprot1.h"
#include "winproto.h"

extern "C" XP_Bool FE_UseExternalProtocolModule(MWContext *pContext, FO_Present_Types iFormatOut, URL_Struct *pURL,
	Net_GetUrlExitFunc *pExitFunc)	{
//	Purpose:	See if we're supposed to handle a specific protocol differently.
//	Arguments:	pContext	The context.
//				iFormatOut	The format out (possibly saving).
//				pURL	The URL to load.
//				pExitFunc	The URL exit routine.  If we handle the protocol, we use the function.
//	Returns:	BOOL	TRUE	We want to handle the protocol here.
//						FALSE	Netlib continues to handle the protocol.
//	Comments:	To work with certain DDE topics which can cause URLs of a specific protocol type to be handled by another
//					application.
//	Revision History:
//		01-17-95	created GAB
//

	//	Extract the protocol.
	if(pURL->address == NULL)	{
		return(FALSE);
	}
	CString csProtocol = pURL->address;
	csProtocol = csProtocol.SpanExcluding(":");
	
	//	See if there's a protocol handler.
	CProtocolItem *pItem = CProtocolItem::Find(csProtocol);
	if(pItem == NULL)	{
		return(FALSE);
	}
    TRACE("External protocol handler being used.\n");

	//	Have the handler attempt, respect it's return value.
	//	Call the URL exit routine if it works.
	if((TwoByteBool)TRUE == pItem->OpenURL(pURL, pContext, iFormatOut))	{
		pExitFunc(pURL, MK_DATA_LOADED, pContext);
	}
	else	{
        return(FALSE);
	}
	return(TRUE);
}

//	Static variables.
CPtrList CProtocolRegistry::m_Registry;

CProtocolRegistry::CProtocolRegistry()  {
//	Purpose:    Construct a registry item.
//	Arguments:  void
//	Returns:    none
//	Comments:   Simply add to this object to the registry list/NOT.
//              Moved to be done in topmost derived classes, since this pointer dynamically changes!
//	Revision History:
//      02-18-95    created
//
}

CProtocolRegistry::~CProtocolRegistry() {
//	Purpose:    Destruct a registry item.
//	Arguments:  void
//	Returns:    none
//	Comments:   Simply remove this object from the registry.
//	Revision History:
//      02-18-95    created
//
    TRACE("Removing protocol from the registry\n");
    m_Registry.RemoveAt(m_rIndex);
}

CProtocolItem::CProtocolItem(CString& csProtocol, CString& csServiceName, int iType) : CProtocolRegistry()  {
//	Purpose:    Register a new protocol Item, with protocol handling, service name, and service type.
//	Arguments:  csProtocol  The protocol the attemp to handle.
//              csServiceName   The name of the service handler for the protocol.
//              iType   The type of service handler, usually DDE or OLE.
//	Returns:    none
//	Comments:   Uses base class to keep track of itself in the registry.
//	Revision History:
//      02-18-95    created
//
    TRACE("Protocol Item %s:%s:%d adding\n", (const char *)csProtocol, (const char *)csServiceName, iType);
    m_csProtocol = csProtocol;
    NET_AddExternalURLType((char *)(const char *)m_csProtocol);
    m_csServiceName = csServiceName;
    m_iType = iType;
}

CProtocolItem::~CProtocolItem() {
//	Purpose:    Destruct a protocol item.
//	Arguments:  void
//	Returns:    none
//	Comments:   nothing special.
//	Revision History:
//      02-18-95    created GAB
//
    //  Do nothing, unless special cleanup required.
    NET_DelExternalURLType((char *)(const char *)m_csProtocol);
    TRACE("Protocol Item %s:%s:%d removing\n", (const char *)m_csProtocol, (const char *)m_csServiceName, m_iType);
}

int CProtocolItem::GetType()    {
//	Purpose:    Determine the type of the protocol Item.
//	Arguments:  void
//	Returns:    int The enumerated type.
//	Comments:
//	Revision History:
//      02-18-95    created GAB
//
    TRACE("Protocol Item type is %d\n", m_iType);
    return(m_iType);
}

CString CProtocolItem::GetServiceName() {
//	Purpose:    Return the service name for the protocol item.
//	Arguments:  void
//	Returns:    CString The service name.
//	Comments:
//	Revision History:
//      02-18-95    created GAB
//

    TRACE("Protocol Item service name is %s\n", (const char *)m_csServiceName);
    return(m_csServiceName);
}

CProtocolItem *CProtocolItem::Find(CString& csProtocol)	{
//	Purpose:	Search for the protocol in the registry.
//	Arguments:	csProtocol, the protocol to search for.
//	Returns:	CProtocolItem *	A pointer to the registered handler for that protocol,
//					or NULL if none found.
//	Comments:	This is a static function.
//	Revision History:
//		01-18-95	created GAB
//

	//	Just loop through the registered protocols, and find the one we're looking for.
	//	Consider them case sensitive, need exact match.
	CProtocolItem *pRetval = NULL;
	POSITION rIndex = m_Registry.GetHeadPosition();
	while(rIndex != NULL)	{
		pRetval = (CProtocolItem *)m_Registry.GetNext(rIndex);
		if(csProtocol == pRetval->m_csProtocol)	{
			break;
		}
		else	{
			pRetval = NULL;
		}
	}
	
	return(pRetval);
}

COLEProtocolItem::COLEProtocolItem(CString& csProtocol, CString& csServiceName) : CProtocolItem(csProtocol, csServiceName, m_OLE)   {
//	Purpose:    Construct an OLE protocol item.
//	Arguments:  csProtocol  The protocol handler type
//              csServiceName   The service name (automation server) of the protocol
//	Returns:    none
//	Comments:   Used to initialize the type in the base class.
//	Revision History:
//      02-18-95    created GAB
//
    TRACE("Creating an OLE protocol handler\n");
    m_rIndex = m_Registry.AddTail(this);
}

BOOL COLEProtocolItem::OLERegister(CString& csProtocol, CString& csServiceName) {
//	Purpose:    Attempt to register an OLE protocol handler.
//	Arguments:  csProtocol  The protocol the handle.
//              csServiceName   The OLE Automation Server to which create the dispatch to when we need to override the protocol.
//	Returns:    BOOL    TRUE    Registered.
//                      FALSE   Not registered.
//	Comments:   Returns FALSE if ther is another registered protocol handler.
//	Revision History:
//      02-17-95    created GAB
//

    //  See if anyone's already handling the protocol.
    if(NULL != Find(csProtocol))    {
        return(FALSE);
    }

    //  Otherwise, we need to register this service to do it's job.
    COLEProtocolItem *pDontCare = new COLEProtocolItem(csProtocol, csServiceName);

    //  Also, we need to save this information into the INI file.
    theApp.WriteProfileString("Automation Protocols", csProtocol, csServiceName);

    return(TRUE);
}

BOOL COLEProtocolItem::OLEUnRegister(CString& csProtocol, CString& csServiceName)   {
//	Purpose:	Attempt to unregister a protocol handler.
//	Arguments:	csProtocol	The protocol to not handle.
//				csServiceName	Should be the currently registered handler name.
//	Returns:	BOOL	TRUE	could unregister.
//						FALSE	couldn't unregister, as was never registered.
//	Comments:
//	Revision History:
//		02-17-95	created GAB
//

	//	Attempt to find the current handler.
	CProtocolItem *pItem = Find(csProtocol);
	if(pItem == NULL)	{
		return(FALSE);
	}
	
	//	See if it's OLE.
	if(pItem->GetType() != m_OLE)	{
		return(FALSE);
	}
	
	//	See if it's the same name.
	COLEProtocolItem *pOLEItem = (COLEProtocolItem *)pItem;
	if(pOLEItem->GetServiceName() != csServiceName)	{
		return(FALSE);
	}
	
	//	Get rid of it.
    //  Out of the INI file too.
	delete pOLEItem;
    theApp.WriteProfileString("Automation Protocols", csProtocol, NULL);

	return(TRUE);
}

BOOL COLEProtocolItem::OpenURL(URL_Struct *pURL, MWContext *pContext, FO_Present_Types iFormatOut)	{
//	Purpose:	Have a OLE service name open up a URL.
//	Arguments:	pURL	The url to open.
//				pContext	The context in which it is opening.
//				iFormatOut	The format out.
//	Returns:	BOOL	TRUE	can open.
//						FALSE	can't open; the server won't talk.
//	Comments:	virtual handler for OpenURL resolves to correct class for protocol handling.
//	Revision History:
//		02-17-95	created GAB
//

    //  Only handle, FO_PRESENT types for now.
    if((iFormatOut & FO_PRESENT) != FO_PRESENT)    {
        return(FALSE);
    }

    //  Start up a conversation with our protocol handler.
    IIProtocol1 ProtocolHandler;
    TRY {
        ProtocolHandler.CreateDispatch(m_csServiceName);
        if(ProtocolHandler.Initialize(m_csProtocol, pURL->address) == 0)    {
            return(FALSE);
        }
        ProtocolHandler.Open(pURL->address);
    }
    CATCH(CException, e)    {   //  Any exception will do
        
        //  Tell the user that we were unable to complete the operation, ask them if they would like to unregister the handler.
        TRACE("Couldn't connect to the OLE automation server\n");
        CString csMessage = szLoadString(IDS_OLE_CANTUSE_HANDLER);
        csMessage += m_csServiceName;
        csMessage += ".  ";
        csMessage += szLoadString(IDS_OLE_CANTUSE_VIEWER2);

        if(IDNO == AfxMessageBox(csMessage, MB_YESNO))   {
            //  They don't want to use the handler in the future.
            //  Unregister it.
            OLEUnRegister(m_csProtocol, m_csServiceName);
        }
        return(FALSE);
    }
    END_CATCH;

    return(TRUE);
}

CDDEProtocolItem::CDDEProtocolItem(CString& csProtocol, CString& csServiceName) : CProtocolItem(csProtocol, csServiceName, m_DDE)   {
//	Purpose:    Construct an DDE protocol item.
//	Arguments:  csProtocol  The protocol handler type
//              csServiceName   The service name (DDE server) of the protocol
//	Returns:    none
//	Comments:   Used to initialize the type in the base class.
//	Revision History:
//      02-18-95    created GAB
//
    TRACE("Creating a DDE protocol handler\n");
    m_rIndex = m_Registry.AddTail(this);
}

BOOL CDDEProtocolItem::DDERegister(CString& csProtocol, CString& csServiceName)	{
//	Purpose:	Attempt to register a DDE protocol handler.
//	Arguments:	csProtocol	The protocol to handle.
//				csServiceName	The DDE service name of the handling applciation.
//	Returns:	BOOL	TRUE	Registered.
//						FALSE	Not registered.
//	Comments:	Only returns FALSE if there is an already registered protcol handler.
//	Revision History:
//		01-18-95	created GAB
//

	//	This is really easy.
	//	If we find a prtocol handler for this protocl, then we can't do it.
	if(NULL != Find(csProtocol))	{
		return(FALSE);
	}
	
	//	Otherwise, simply create a new protocol item.
	CDDEProtocolItem *pDontCare = new CDDEProtocolItem(csProtocol, csServiceName);
	if(pDontCare != NULL)	{
		return(TRUE);
	}
	return(FALSE);
}

BOOL CDDEProtocolItem::DDEUnRegister(CString& csProtocol, CString& csServiceName)	{
//	Purpose:	Attempt to unregister a protocol handler.
//	Arguments:	csProtocol	The protocol to not handle.
//				csServiceName	Should be the currently registered handler name.
//	Returns:	BOOL	TRUE	could unregister.
//						FALSE	couldn't unregister, as was never registered.
//	Comments:
//	Revision History:
//		01-18-95	created GAB
//

	//	Attempt to find the current handler.
	CProtocolItem *pItem = Find(csProtocol);
	if(pItem == NULL)	{
		return(FALSE);
	}
	
	//	See if it's DDE.
	if(pItem->GetType() != m_DDE)	{
		return(FALSE);
	}
	
	//	See if it's the same name.
	CDDEProtocolItem *pDDEItem = (CDDEProtocolItem *)pItem;
	if(pDDEItem->GetServiceName() != csServiceName)	{
		return(FALSE);
	}
	
	//	Get rid of it.
	delete pDDEItem;
	return(TRUE);
}

BOOL CDDEProtocolItem::OpenURL(URL_Struct *pURL, MWContext *pContext, FO_Present_Types iFormatOut)	{
//	Purpose:	Have a DDE service name open up a URL.
//	Arguments:	pURL	The url to open.
//				pContext	The context in which it is opening.
//				iFormatOut	The format out, wether to save or not basically.
//	Returns:	BOOL	TRUE	can open.
//						FALSE	can't open; the server won't talk.
//	Comments:	virtual handler for OpenURL resolves to correct class for protocol handling.
//				Calls back into DDE code for actual opening.
//	Revision History:
//		01-18-95	created GAB
//

	//	Just call the DDE implementation to open up the URL.
	//	Be sure to let them know the service name.
	return(CDDEWrapper::OpenURL(m_csProtocol, m_csServiceName, pURL, pContext, iFormatOut));
}
