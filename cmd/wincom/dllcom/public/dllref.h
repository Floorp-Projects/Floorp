/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __DllRefCounterHelper_H
#define __DllRefCounterHelper_H

//  Here's a helper to keep DLL ref counting straight.
//  Just use it as a member to your classes which control the lifetime
//      of the DLL instance.
class CRefDll   {
private:
    IUnknown *m_pDllRef;
public:
    CRefDll()   {
        m_pDllRef = (IUnknown *)CProcess::GetProcessDll();
    }
    ~CRefDll()  {
        if(m_pDllRef)   {
            m_pDllRef->Release();
            m_pDllRef = NULL;
        }
    }
};


#endif // __DllRefCounterHelper_H