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
 
#ifndef PreferencesActionEvents_h___
#define PreferencesActionEvents_h___

#include "nsActions.h"

#include "ns_util.h"

typedef struct _peStruct {
    WebShellInitContext *cx;
    jobject obj;
    jobject callback;
} peStruct;

//
// ActionEvent class definitions
//

class wsPrefChangedEvent : public nsActionEvent {
public:
    wsPrefChangedEvent (const char *prefName, peStruct *yourPeStruct);
    void    *       handleEvent    (void);
    
protected:
    const char *mPrefName;
    peStruct *mPeStruct;
};

class wsSetUnicharPrefEvent : public nsActionEvent {
public:
    wsSetUnicharPrefEvent (const char *prefName, const jchar *yourPrefValue);
    void    *       handleEvent    (void);
    
protected:
    const char *mPrefName;
    const jchar *mPrefValue;
};

class wsSetIntPrefEvent : public nsActionEvent {
public:
    wsSetIntPrefEvent (const char *prefName, PRInt32 yourPrefValue);
    void    *       handleEvent    (void);
    
protected:
    const char *mPrefName;
    PRInt32 mPrefValue;
};

class wsSetBoolPrefEvent : public nsActionEvent {
public:
    wsSetBoolPrefEvent (const char *prefName, PRBool yourPrefValue);
    void    *       handleEvent    (void);
    
protected:
    const char *mPrefName;
    PRBool mPrefValue;
};

class wsGetPrefsEvent : public nsActionEvent {
public:
    wsGetPrefsEvent (peStruct *yourPeStruct);
    void    *       handleEvent    (void);
    
protected:
    peStruct *mPeStruct;
};

class wsRegisterPrefCallbackEvent : public nsActionEvent {
public:
    wsRegisterPrefCallbackEvent (const char * prefName, 
				 peStruct *yourPeStruct);
    void    *       handleEvent    (void);
    
protected:
  const char *mPrefName;
    peStruct *mPeStruct;
};


#endif /* PreferencesActionEvents_h___ */

      
// EOF
