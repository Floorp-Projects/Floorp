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

#include "presentm.h"

#include "strmdata.h"
#include "olectc.h"
#include "ddectc.h"
#include "helper.h"

//	All our registered ContentTypeConverters
extern "C" BOOL WPM_RegisterContentTypeConverter(char *pFormatIn,
	FO_Present_Types iFormatOut, void *vpDataObject,
	NET_Converter *pConverterFunc, BOOL bAutomated)	{

	NET_RegContentTypeConverter (pFormatIn,iFormatOut,vpDataObject, pConverterFunc,
									bAutomated);
	return(TRUE);
}

CStreamData *WPM_UnRegisterContentTypeConverter(const char *pServer,
	const char *pMimeType, FO_Present_Types iFormatOut)	{
//	Purpose:	Remove a content type converter from the list of registered
//						types.
//	Arguments:	pServer	The name of the server.
//						pMimeType	The mime type the server should be
//							registered to handle.
//						iFormatOut	The format out that the server is registered
//							to handle.
//	Returns:	CStreamData *	The data passed in via RegisterContentTypeConverter,
//						so the application can free it.  NULL on failure.
//	Comments:	This function has intimate knowledge of the CStreamData
//							class and it's heirs.  This is so that it can correctly
//							find the server in the registration list.
//				Only automated converters can be unregistered.
//	Revision History:
//		01-08-94	created GAB
//

    //  Can't handle any caching formats, shouldn't ever be registered!
    if((iFormatOut & FO_CACHE_ONLY) || (iFormatOut & FO_ONLY_FROM_CACHE))  {
        ASSERT(0);
        return(NULL);
    }


    XP_List* pList = NET_GetRegConverterList(iFormatOut);
    void *objPtr = NULL;
    CStreamData *pAutoStream = (CStreamData *)NET_GETDataObject(pList, (char *)pMimeType, &objPtr);
    if(pAutoStream) {
        switch(pAutoStream->GetType())	{
        case CStreamData::m_DDE:	{
            CDDEStreamData *pDDEStream = (CDDEStreamData *)pAutoStream;

            //	Compare the server names.
            //	This will not be a case sensitive thing, since DDE isn't
            //		case sensitive.
            if(0 == pDDEStream->m_csServerName.CompareNoCase(pServer)){
                //	This is the one.  Take it out.
                XP_ListRemoveObject(pList, objPtr);
                XP_DELETE(objPtr);
                objPtr = NULL;
                return(pAutoStream);
            }
            break;
                                    }
        case CStreamData::m_OLE:	{
            COLEStreamData *pOLEStream = (COLEStreamData *)pAutoStream;

            //  Compare the server names.
            //  This will be a case sensitive thing.
            if(pOLEStream->m_csServerName == pServer)   {
                //  This is the one.  Take it out.
                XP_ListRemoveObject(pList, objPtr);
                XP_DELETE(objPtr);
                objPtr = NULL;                    
                return(pAutoStream);
            }
            break;
                                    }
        default:
            //	unknown type.
            ASSERT(0);
            break;
        }
    }

    //	Not successful.
    return(NULL);
}
