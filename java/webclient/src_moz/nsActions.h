/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 */

/*
 * nsActions.h
 */
 
#ifndef nsActions_h___
#define nsActions_h___

#include "plevent.h"

/**

 * Concrete subclasses of nsActionEvent are used to safely convey an
 * action from Java to mozilla.

 * This class is kind of like a subclass of the C based PLEvent struct,
 * defined in
 * <http://lxr.mozilla.org/mozilla/source/xpcom/threads/plevent.h#455>.

 * Lifecycle

 * nsActionEvent subclass instances are usually created in a JNI method
 * implementation, such as
 * Java_org_mozilla*NavigationImpl_nativeLoadURL().  In nativeLoadURL(),
 * we create an instance of wsLoadURLEvent, which is a subclass of
 * nsActionEvent, passing in the nsIWebShell instance, and the url
 * string.  The arguments to the nsActionEvent constructor vary from one
 * subclass to the next, but they all have the property that they
 * provide information used in the subclass's handleEvent()
 * method.

 * The nsActionEvent subclass is then cast to a PLEvent struct, and
 * passed into either util_PostEvent() or util_PostSynchronous event,
 * declared in ns_util.h.  See the comments for those functions for
 * information on how they deal with the nsActionEvent, cast as a
 * PLEvent.

 * During event processing in util_Post*Event, the subclass's
 * handleEvent() method is called.  This is where the appropriate action
 * for that subclass takes place.  For example for wsLoadURLEvent,
 * handleEvent calls nsIWebShell::LoadURL().

 * The life of an nsActionEvent subclass ends shortly after the
 * handleEvent() method is called.  This happens when mozilla calls
 * nsActionEvent::destroyEvent(), which simply deletes the instance.

 */

class nsActionEvent {
public:
                        nsActionEvent  ();
        virtual        ~nsActionEvent  () {};
        virtual void *  handleEvent    (void) = 0; //{ return nsnull;};
                void    destroyEvent   (void) { delete this; };
            operator    PLEvent*       ()     { return &mEvent; };

protected:
        PLEvent         mEvent;
};





#endif /* nsActions_h___ */

      
// EOF
