/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
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

#ifndef __EmbedEventHandling__
#define __EmbedEventHandling__

#include "LAttachment.h"
#include "LPeriodical.h"

class nsIWidget;
class nsIEventSink;


//*****************************************************************************
// class CEmbedEventAttachment
//
// Handles events for windows popped up by Gecko which are not LWindows
//
//*****************************************************************************   

class	CEmbedEventAttachment : public LAttachment
{
public:

	enum { class_ID = FOUR_CHAR_CODE('GAWA') };
				
						        CEmbedEventAttachment();	
	virtual				        ~CEmbedEventAttachment();
	
	// LAttachment
	virtual	void                ExecuteSelf(MessageT		inMessage,
                                            void*			ioParam);
										              
protected:
    Boolean                     IsAlienGeckoWindow(WindowPtr inMacWindow);
  
    // utility routines for getting the toplevel widget and event sink
    // stashed in the properties of gecko windows.
    static void GetWindowEventSink ( WindowPtr aWindow, nsIEventSink** outSink ) ;
    static void GetTopWidget ( WindowPtr aWindow, nsIWidget** outWidget ) ;

    static WindowPtr            mLastAlienWindowClicked;
};


//*****************************************************************************
// class CEmbedIdler
//
// LPeriodical idler which calls Gecko's idling
//
//*****************************************************************************   

class CEmbedIdler : public LPeriodical
{
public:
                                CEmbedIdler();
    virtual                     ~CEmbedIdler();
  
  // LPeriodical                      
	virtual	void		        SpendTime(const EventRecord& inMacEvent);
};


//*****************************************************************************
// class CEmbedRepeater
//
// LPeriodical repeater which calls Gecko's repeaters
//
//*****************************************************************************   

class CEmbedRepeater : public LPeriodical
{
public:
                                CEmbedRepeater();
    virtual                     ~CEmbedRepeater();
  
  // LPeriodical                      
	virtual	void		        SpendTime(const EventRecord& inMacEvent);
};


//*****************************************************************************
// Initialization Function - Call at application startup. 
//*****************************************************************************

void InitializeEmbedEventHandling(LApplication* theApplication);   

#endif // __EmbedEventHandling__
