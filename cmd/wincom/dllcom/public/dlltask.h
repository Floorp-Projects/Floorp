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

#ifndef __DllProcessData_H
#define __DllProcessData_H

//  THIS HEADER IS NOT PUBLIC TO OTHER CODE EXCEPT THE
//      CORE DLL IMPLEMENTATION.

//  Classes used to maintain a seperate instace list of
//      Dll data for each different task utilizing the
//      Dll.  The goal here is to transparently make
//      win16 Dlls which share data, to act like win32
//      Dlls, which do not share data.

//  This class is solely contained by the CComDll class
//      constructor and destructor, but this class
//      registers itself into a list such that a per
//      process lookup can find a CComDll instance.
class CProcessEntry  {
    friend class CComDll;
    friend class CProcess;
private:
    //  Used in conjunction with the CProcess list
    //      to automatically manage it.
    CProcessEntry(CComDll *pDll);
    ~CProcessEntry();

private:
    //  All needed state data to seperate out and
    //      combine a single process with a single
    //      set of instance data.
    DWORD m_dwProcessID;
    CComDll *m_pDllInstance;
    CProcessEntry *m_pNext;
};

//  This class is never actually allocated, but is used
//      to logically seperate out the members and their
//      purpose.
class CProcess  {
    friend class CProcessEntry;
private:
    //  A list of process IDs and Dll instance data.
    static CProcessEntry *m_pProcessList;

public:
    static CComDll *GetProcessDll();
    static BOOL CanUnloadNow();
    static DWORD GetProcessID();
};

#endif // __DllProcessData_H