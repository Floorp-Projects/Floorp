/* $Id: qtfe-timer.cpp,v 1.1 1998/09/25 18:01:38 ramiro%netscape.com Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include <fe_proto.h>

#include <qobject.h>

/* This class receives timers and calls global functions */

class InternalTimer : public QObject {
public:
    InternalTimer( TimeoutCallbackFunction cb, void *cl, int msecs )
	: QObject(0), callback(cb), closure(cl)
    {
	startTimer(msecs);
    }
    ~InternalTimer()
    {
    }
protected:
    void timerEvent( QTimerEvent *e )
    { 
	killTimer(e->timerId()); 
	(*callback)(closure); 
	delete this;
    }
private:
    TimeoutCallbackFunction callback;
    void *closure;
};

extern "C" 
void * FE_SetTimeout(TimeoutCallbackFunction func, void * closure,
                     uint32 msecs)
{
    return new InternalTimer( func, closure, msecs );
}

extern "C" 
void FE_ClearTimeout(void *timer_id)
{
    delete (InternalTimer*)timer_id;
}
