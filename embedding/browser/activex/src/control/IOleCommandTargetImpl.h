/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 */
#ifndef IOLECOMMANDIMPL_H
#define IOLECOMMANDIMPL_H

// Implementation of the IOleCommandTarget interface. The template is
// reasonably generic and reusable which is a good thing given how needlessly
// complicated this interface is. Blame Microsoft for that and not me.
//
// To use this class, derive your class from it like this:
//
// class CComMyClass : public IOleCommandTargetImpl<CComMyClass>
// {
//     ... Ensure IOleCommandTarget is listed in the interface map ...
// BEGIN_COM_MAP(CComMyClass)
//     COM_INTERFACE_ENTRY(IOleCommandTarget)
//     // etc.
// END_COM_MAP()
//     ... And then later on define the command target table ...
// BEGIN_OLECOMMAND_TABLE()
//    OLECOMMAND_MESSAGE(OLECMDID_PRINT, NULL, ID_PRINT, L"Print", L"Print the page")
//    OLECOMMAND_MESSAGE(OLECMDID_SAVEAS, NULL, 0, L"SaveAs", L"Save the page")
//    OLECOMMAND_HANDLER(IDM_EDITMODE, &CGID_MSHTML, EditModeHandler, L"EditMode", L"Switch to edit mode")
// END_OLECOMMAND_TABLE()
//     ... Now the window that OLECOMMAND_MESSAGE sends WM_COMMANDs to ...
//     HWND GetCommandTargetWindow() const
//     {
//         return m_hWnd;
//     }
//     ... Now procedures that OLECOMMAND_HANDLER calls ...
//     static HRESULT _stdcall EditModeHandler(CMozillaBrowser *pThis, const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut);
// }
//
// The command table defines which commands the object supports. Commands are
// defined by a command id and a command group plus a WM_COMMAND id or procedure,
// and a verb and short description.
//
// Notice that there are two macros for handling Ole Commands. The first,
// OLECOMMAND_MESSAGE sends a WM_COMMAND message to the window returned from
// GetCommandTargetWindow() (that the derived class must implement if it uses
// this macro).
//
// The second, OLECOMMAND_HANDLER calls a static handler procedure that
// conforms to the OleCommandProc typedef. The first parameter, pThis means
// the static handler has access to the methods and variables in the class
// instance.
//
// The OLECOMMAND_HANDLER macro is generally more useful when a command
// takes parameters or needs to return a result to the caller. 
//
template< class T >
class IOleCommandTargetImpl : public IOleCommandTarget
{
    struct OleExecData
    {
        const GUID *pguidCmdGroup;
        DWORD nCmdID;
        DWORD nCmdexecopt;
        VARIANT *pvaIn;
        VARIANT *pvaOut;
    };

public:
    typedef HRESULT (_stdcall *OleCommandProc)(T *pT, const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut);

    struct OleCommandInfo
    {
        ULONG            nCmdID;
        const GUID        *pCmdGUID;
        ULONG            nWindowsCmdID;
        OleCommandProc    pfnCommandProc;
        wchar_t            *szVerbText;
        wchar_t            *szStatusText;
    };

    // Query the status of the specified commands (test if is it supported etc.)
    virtual HRESULT STDMETHODCALLTYPE QueryStatus(const GUID __RPC_FAR *pguidCmdGroup, ULONG cCmds, OLECMD __RPC_FAR prgCmds[], OLECMDTEXT __RPC_FAR *pCmdText)
    {
        T* pT = static_cast<T*>(this);
        
        if (prgCmds == NULL)
        {
            return E_INVALIDARG;
        }

        OleCommandInfo *pCommands = pT->GetCommandTable();
        NG_ASSERT(pCommands);

        BOOL bCmdGroupFound = FALSE;
        BOOL bTextSet = FALSE;

        // Iterate through list of commands and flag them as supported/unsupported
        for (ULONG nCmd = 0; nCmd < cCmds; nCmd++)
        {
            // Unsupported by default
            prgCmds[nCmd].cmdf = 0;

            // Search the support command list
            for (int nSupported = 0; pCommands[nSupported].pCmdGUID != &GUID_NULL; nSupported++)
            {
                OleCommandInfo *pCI = &pCommands[nSupported];

                if (pguidCmdGroup && pCI->pCmdGUID && memcmp(pguidCmdGroup, pCI->pCmdGUID, sizeof(GUID)) == 0)
                {
                    continue;
                }
                bCmdGroupFound = TRUE;

                if (pCI->nCmdID != prgCmds[nCmd].cmdID)
                {
                    continue;
                }

                // Command is supported so flag it and possibly enable it
                prgCmds[nCmd].cmdf = OLECMDF_SUPPORTED;
                if (pCI->nWindowsCmdID != 0)
                {
                    prgCmds[nCmd].cmdf |= OLECMDF_ENABLED;
                }

                // Copy the status/verb text for the first supported command only
                if (!bTextSet && pCmdText)
                {
                    // See what text the caller wants
                    wchar_t *pszTextToCopy = NULL;
                    if (pCmdText->cmdtextf & OLECMDTEXTF_NAME)
                    {
                        pszTextToCopy = pCI->szVerbText;
                    }
                    else if (pCmdText->cmdtextf & OLECMDTEXTF_STATUS)
                    {
                        pszTextToCopy = pCI->szStatusText;
                    }
                    
                    // Copy the text
                    pCmdText->cwActual = 0;
                    memset(pCmdText->rgwz, 0, pCmdText->cwBuf * sizeof(wchar_t));
                    if (pszTextToCopy)
                    {
                        // Don't exceed the provided buffer size
                        int nTextLen = wcslen(pszTextToCopy);
                        if (nTextLen > pCmdText->cwBuf)
                        {
                            nTextLen = pCmdText->cwBuf;
                        }

                        wcsncpy(pCmdText->rgwz, pszTextToCopy, nTextLen);
                        pCmdText->cwActual = nTextLen;
                    }
                    
                    bTextSet = TRUE;
                }
                break;
            }
        }
        
        // Was the command group found?
        if (!bCmdGroupFound)
        {
            OLECMDERR_E_UNKNOWNGROUP;
        }

        return S_OK;
    }


    // Execute the specified command
    virtual HRESULT STDMETHODCALLTYPE Exec(const GUID __RPC_FAR *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT __RPC_FAR *pvaIn, VARIANT __RPC_FAR *pvaOut)
    {
        T* pT = static_cast<T*>(this);
        BOOL bCmdGroupFound = FALSE;

        OleCommandInfo *pCommands = pT->GetCommandTable();
        NG_ASSERT(pCommands);

        // Search the support command list
        for (int nSupported = 0; pCommands[nSupported].pCmdGUID != &GUID_NULL; nSupported++)
        {
            OleCommandInfo *pCI = &pCommands[nSupported];

            if (pguidCmdGroup && pCI->pCmdGUID && memcmp(pguidCmdGroup, pCI->pCmdGUID, sizeof(GUID)) == 0)
            {
                continue;
            }
            bCmdGroupFound = TRUE;

            if (pCI->nCmdID != nCmdID)
            {
                continue;
            }

            // Send ourselves a WM_COMMAND windows message with the associated
            // identifier and exec data
            OleExecData cData;
            cData.pguidCmdGroup = pguidCmdGroup;
            cData.nCmdID = nCmdID;
            cData.nCmdexecopt = nCmdexecopt;
            cData.pvaIn = pvaIn;
            cData.pvaOut = pvaOut;

            if (pCI->pfnCommandProc)
            {
                pCI->pfnCommandProc(pT, pCI->pCmdGUID, pCI->nCmdID, nCmdexecopt, pvaIn, pvaOut);
            }
            else if (pCI->nWindowsCmdID != 0 &&
                     !(nCmdexecopt & OLECMDEXECOPT_SHOWHELP))
            {
                HWND hwndTarget = pT->GetCommandTargetWindow();
                if (hwndTarget)
                {
                    ::SendMessage(hwndTarget, WM_COMMAND, LOWORD(pCI->nWindowsCmdID), (LPARAM) &cData);
                }
            }
            else
            {
                // Command supported but not implemented
                continue;
            }

            return S_OK;
        }

        // Was the command group found?
        if (!bCmdGroupFound)
        {
            OLECMDERR_E_UNKNOWNGROUP;
        }

        return OLECMDERR_E_NOTSUPPORTED;
    }
};

// Macros to be placed in any class derived from the IOleCommandTargetImpl
// class. These define what commands are exposed from the object.

#define BEGIN_OLECOMMAND_TABLE() \
    OleCommandInfo *GetCommandTable() \
    { \
        static OleCommandInfo s_aSupportedCommands[] = \
        {

#define OLECOMMAND_MESSAGE(id, group, cmd, verb, desc) \
            { id, group, cmd, NULL, verb, desc },

#define OLECOMMAND_HANDLER(id, group, handler, verb, desc) \
            { id, group, 0, handler, verb, desc },

#define END_OLECOMMAND_TABLE() \
            { 0, &GUID_NULL, 0, NULL, NULL, NULL } \
        }; \
        return s_aSupportedCommands; \
    };

#endif