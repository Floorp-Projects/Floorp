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

#ifndef __PresentationManagerStuff_H
//	Avoid include redundancy
//
#define __PresentationManagerStuff_H

//	Purpose:	Intercept all calls network streaming and decide
//					what to do with them in the the windows
//					client.
//	Comments:	This is needed to support all the funky ways that
//					automation applications (either DDE or OLE)
//					want to dynamically have us send data of
//					different types to different places.
//				what a nightmare.  Really should have had this
//					type of functionality in the XP code since the
//					beginning.
//	Revision History:
//		01-03-95	created GAB
//

//	Required Includes
//

//	Constants
//

//	Structures
//

// the struct is the same as the one in libnet mkstream.c's
// net_ConverterStruct, we need to ask libnet to publish the struct.
typedef struct ContentTypeConverter {
     XP_List       * converter_stack;  /* the ordered list of converters;
                                        * elements are net_ConverterElement's
                                        */
     char          * format_in;        /* the input (mime) type that the 
										* function accepts 
										*/
     char          * encoding_in;      /* the input content-encoding that the
										* function accepts, or 0 for `any'.
										*/
     FO_Present_Types format_out;      /* the output type of the function */
	 Bool			bAutomated;			/* this flag currently only used by Window on the
											client side. */
} net_ConverterStruct;


//	Global variables
//

//	Macros
//

//	Function declarations
//
CStreamData *WPM_UnRegisterContentTypeConverter(const char *pServer,
	const char *pMimeType, FO_Present_Types iFormatOut);	

extern "C" BOOL WPM_RegisterContentTypeConverter(char *pFormatIn,
	FO_Present_Types iFormatOut, void *vpDataObject,
	NET_Converter *pConverterFunc, BOOL bAutomated);

#endif // __PresentationManagerStuff_H
