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

#ifndef _MsgBg_H_
#define _MsgBg_H_

#include "msgzap.h"

class MSG_Pane;

class msg_Background : public MSG_ZapIt {
public:
    msg_Background();
    virtual ~msg_Background();

    /* Begin() kicks things off.  Sometime after making this call, calls will
       be made to DoSomeWork().  This will interrupt any current background
       operation or URL running on the given MSG_Pane. */
    virtual int Begin(MSG_Pane* pane);


    /* Interrupt() interrupts the running background operation.  Just does an
       InterruptContext on the pane's context. */
    virtual void Interrupt();

    /* Whether we are currently doing our background operation. */
    virtual XP_Bool IsRunning();


	// This routine is called from netlib (via msgglue) to cause us to actually
	// do something.
	static int ProcessBackground(URL_Struct* urlstruct);

protected:

	static msg_Background* FindBGObj(URL_Struct* urlstruct);

    static void PreExit_s(URL_Struct* urlstruct, int status,
						  MWContext* context);
    virtual void PreExit(URL_Struct* urlstruct, int status,
						 MWContext* context);



    /* The below are the only routines typically redefined by subclasses. */


    /* DoSomeWork() keeps getting called.  If it returns
       MK_WAITING_FOR_CONNECTION, that means it hasn't finished its stuff.  If
       it returns MK_CONNECTED, that means it has successfully finished.  If it
       returns a negative value, that means we failed and it's an error
       condition. */
    virtual int DoSomeWork() = 0;


    /* AllDone() gets called when things are finished.  If the given status is
       negative, then we were interrupted or had an error.  This is a good
       place to kick off another background operation.  If it returns TRUE,
	   then this background object will be destroyed. */
    virtual XP_Bool AllDone(int status);

	MSG_Pane* m_pane;
	URL_Struct* m_urlstruct;

};


#endif /* _MsgBg_H_ */
