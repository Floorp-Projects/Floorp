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

//this is for the dialog box itself to get a cheap membervariable and access methods. 
//to limit the code changes in the dialogs themselves
//derive your dialog from as many of these as you need. 

#ifndef _CALLBACK_H
#define _CALLBACK_H

class CWFECallbacks
{
    IWFEInterface *pwfeInterface;
public:
    CWFECallbacks(){pwfeInterface=NULL;}
    void setWFE(IWFEInterface *wfe){pwfeInterface=wfe;}
    IWFEInterface *getWFE(){return pwfeInterface;}
};

class CEDTCallbacks
{
    IEDTInterface *pedtInterface;
public:
    CEDTCallbacks(){pedtInterface=NULL;}
    void setEDT(IEDTInterface *edt){pedtInterface=edt;}
    IEDTInterface *getEDT(){return pedtInterface;}
};
#endif //_CALLBACK_H


