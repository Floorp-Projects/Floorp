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
 * Contributor(s): 
 */
#include "StdAfx.h"

#include "MozillaBrowser.h"
#include "PromptService.h"

#include "nsCOMPtr.h"

#include "nsIPromptService.h"
#include "nsIFactory.h"
#include "nsIDOMWindow.h"
#include "nsReadableUtils.h"

class PromptDlg
{
public:
    PromptDlg();
    virtual ~PromptDlg();

    nsresult ConfirmCheck(
        HWND hwndParent,
        const PRUnichar *dialogTitle,
        const PRUnichar *text,
        const PRUnichar *checkMsg, PRBool *checkValue,
        PRBool *_retval);

    nsresult ConfirmEx(
        HWND hwndParent,
        const PRUnichar *dialogTitle,
        const PRUnichar *text,
        PRUint32 buttonFlags,
        const PRUnichar *button0Title,
        const PRUnichar *button1Title,
        const PRUnichar *button2Title,
        const PRUnichar *checkMsg, PRBool *checkValue,
        PRInt32 *buttonPressed);

    nsresult Prompt(HWND hwndParent, const PRUnichar *dialogTitle,
        const PRUnichar *text, PRUnichar **value,
        const PRUnichar *checkMsg, PRBool *checkValue,
        PRBool *_retval);

    nsresult PromptUsernameAndPassword(HWND hwndParent,
        const PRUnichar *dialogTitle,
        const PRUnichar *text,
        PRUnichar **username, PRUnichar **password,
        const PRUnichar *checkMsg, PRBool *checkValue,
        PRBool *_retval);

    nsresult PromptPassword(HWND hwndParent,
        const PRUnichar *dialogTitle,
        const PRUnichar *text,
        PRUnichar **password,
        const PRUnichar *checkMsg, PRBool *checkValue,
        PRBool *_retval);

protected:
    static BOOL CALLBACK ConfirmProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK PromptProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    TCHAR *mTitle;
    TCHAR *mMessage;
    TCHAR *mCheckMessage;
    BOOL   mCheckValue;
    TCHAR *mButtonTitles[3];
    TCHAR *mUsername;
    TCHAR *mPassword;
    TCHAR *mValue;

    enum {
        PROMPT_USERPASS,
        PROMPT_PASS,
        PROMPT_VALUE
    } mPromptMode;
};

PromptDlg::PromptDlg() :
    mMessage(NULL),
    mCheckMessage(NULL),
    mCheckValue(FALSE),
    mUsername(NULL),
    mPassword(NULL),
    mValue(NULL),
    mTitle(NULL),
    mPromptMode(PROMPT_USERPASS)
{
    for (int i = 0; i < 3; i++)
    {
        mButtonTitles[i] = NULL;
    }
}

PromptDlg::~PromptDlg()
{
    if (mTitle)
        free(mTitle);
    if (mMessage)
        free(mMessage);
    if (mCheckMessage)
        free(mCheckMessage);
    if (mUsername)
        free(mUsername);
    if (mPassword)
        free(mPassword);
    if (mValue)
        free(mValue);
    for (int i = 0; i < 3; i++)
    {
        if (mButtonTitles[i])
        {
            free(mButtonTitles[i]);
        }
    }
}

nsresult PromptDlg::ConfirmCheck(
    HWND hwndParent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    PRInt32 buttonPressed = 0;
    PRUint32 buttonFlags = nsIPromptService::BUTTON_TITLE_YES +
                          (nsIPromptService::BUTTON_TITLE_NO << 8);

    // Use the extended confirmation dialog with Yes & No buttons
    nsresult rv = ConfirmEx(hwndParent, dialogTitle, text,
        buttonFlags, NULL, NULL, NULL,
        checkMsg, checkValue, &buttonPressed);

    if (NS_SUCCEEDED(rv))
    {
        *_retval = (buttonPressed == 0) ? PR_TRUE : PR_FALSE;
    }

    return rv;
}

nsresult PromptDlg::ConfirmEx(
    HWND hwndParent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRUint32 buttonFlags,
    const PRUnichar *button0Title,
    const PRUnichar *button1Title,
    const PRUnichar *button2Title,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRInt32 *buttonPressed)
{
    NS_ENSURE_ARG_POINTER(dialogTitle);
    NS_ENSURE_ARG_POINTER(text);
    NS_ENSURE_ARG_POINTER(buttonPressed);

    HINSTANCE hInstResource = _Module.m_hInstResource;

    USES_CONVERSION;

    // Duplicate all strings, turning them into TCHARs

    mTitle = _tcsdup(W2T(dialogTitle));
    mMessage = _tcsdup(W2T(text));
    if (checkMsg)
    {
        NS_ENSURE_ARG_POINTER(checkValue);
        mCheckMessage = _tcsdup(W2T(checkMsg));
        mCheckValue = (*checkValue == PR_TRUE) ? TRUE : FALSE;
    }

    // Turn the button flags into strings. The nsIPromptService flags
    // are scary bit shifted values which explains the masks and bit
    // shifting happening in this loop.

    for (int i = 0; i < 3; i++)
    {
        PRUint32 titleId = buttonFlags & 255;
        TCHAR **title = &mButtonTitles[i];
        switch (titleId)
        {
        case nsIPromptService::BUTTON_TITLE_OK:
        case nsIPromptService::BUTTON_TITLE_CANCEL:
        case nsIPromptService::BUTTON_TITLE_YES:
        case nsIPromptService::BUTTON_TITLE_NO:
        case nsIPromptService::BUTTON_TITLE_SAVE:
        case nsIPromptService::BUTTON_TITLE_DONT_SAVE:
        case nsIPromptService::BUTTON_TITLE_REVERT:
        {
            const int kTitleSize = 256;
            int stringId = IDS_CONFIRMEX_OK + titleId - nsIPromptService::BUTTON_TITLE_OK;
            *title = (TCHAR *) malloc(sizeof(TCHAR) * kTitleSize);
            ::LoadString(hInstResource, stringId, *title, kTitleSize - 1);
            break;
        }
        case nsIPromptService::BUTTON_TITLE_IS_STRING:
        {
            const PRUnichar *srcTitle =
                (i == 0) ? button0Title : 
                (i == 1) ? button1Title : button2Title;
            if (srcTitle)
            {
                *title = _tcsdup(W2T(srcTitle));
            }
            break;
        }
        default:
            // ANYTHING ELSE GETS IGNORED
            break;
        }
        buttonFlags >>= 8;
    }

    // Must have at least one button the user can click on!
    NS_ENSURE_ARG_POINTER(mButtonTitles[0]);

    INT result = DialogBoxParam(hInstResource,
        MAKEINTRESOURCE(IDD_CONFIRMEX), hwndParent, ConfirmProc, (LPARAM) this);

    if (checkValue)
        *checkValue = mCheckValue;

    if (buttonPressed)
        *buttonPressed = result;

    return NS_OK;
}

nsresult
PromptDlg::Prompt(HWND hwndParent, const PRUnichar *dialogTitle,
    const PRUnichar *text, PRUnichar **value,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(text);
    NS_ENSURE_ARG_POINTER(_retval);

    HINSTANCE hInstResource = _Module.m_hInstResource;

    USES_CONVERSION;

    // Duplicate all strings, turning them into TCHARs

    if (dialogTitle)
        mTitle = _tcsdup(W2T(dialogTitle));
    mMessage = _tcsdup(W2T(text));
    if (checkMsg)
    {
        NS_ENSURE_ARG_POINTER(checkValue);
        mCheckMessage = _tcsdup(W2T(checkMsg));
        mCheckValue = (*checkValue == PR_TRUE) ? TRUE : FALSE;
    }
    if (value)
    {
        mValue = _tcsdup(W2T(*value));
    }

    mPromptMode = PROMPT_VALUE;
    INT result = DialogBoxParam(hInstResource,
        MAKEINTRESOURCE(IDD_PROMPT), hwndParent, PromptProc, (LPARAM) this);

    if (result == IDOK)
    {
        if (value)
        {
            if (*value)
                nsMemory::Free(*value);
            nsAutoString v(T2W(mValue));
            *value = ToNewUnicode(v);
        }

        if (checkValue)
            *checkValue = mCheckValue;

        *_retval = TRUE;
    }
    else
    {
        *_retval = FALSE;
    }

    return NS_OK;
}

nsresult
PromptDlg::PromptUsernameAndPassword(HWND hwndParent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRUnichar **username, PRUnichar **password,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(text);
    NS_ENSURE_ARG_POINTER(_retval);

    HINSTANCE hInstResource = _Module.m_hInstResource;

    USES_CONVERSION;

    // Duplicate all strings, turning them into TCHARs

    if (dialogTitle)
        mTitle = _tcsdup(W2T(dialogTitle));
    mMessage = _tcsdup(W2T(text));
    if (checkMsg)
    {
        NS_ENSURE_ARG_POINTER(checkValue);
        mCheckMessage = _tcsdup(W2T(checkMsg));
        mCheckValue = (*checkValue == PR_TRUE) ? TRUE : FALSE;
    }
    if (username)
    {
        mUsername = _tcsdup(W2T(*username));
    }
    if (password)
    {
        mPassword = _tcsdup(W2T(*password));
    }

    mPromptMode = PROMPT_USERPASS;
    INT result = DialogBoxParam(hInstResource,
        MAKEINTRESOURCE(IDD_PROMPTUSERPASS), hwndParent, PromptProc, (LPARAM) this);

    if (result == IDOK)
    {
        if (username)
        {
            if (*username)
                nsMemory::Free(*username);
            nsAutoString user(T2W(mUsername));
            *username = ToNewUnicode(user);
        }
        if (password)
        {
            if (*password)
                nsMemory::Free(*password);
            nsAutoString pass(T2W(mPassword));
            *password = ToNewUnicode(pass);
        }

        if (checkValue)
            *checkValue = mCheckValue;

        *_retval = TRUE;
    }
    else
    {
        *_retval = FALSE;
    }

    return NS_OK;
}

nsresult
PromptDlg::PromptPassword(HWND hwndParent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRUnichar **password,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(text);
    NS_ENSURE_ARG_POINTER(_retval);

    HINSTANCE hInstResource = _Module.m_hInstResource;

    USES_CONVERSION;

    // Duplicate all strings, turning them into TCHARs

    if (dialogTitle)
        mTitle = _tcsdup(W2T(dialogTitle));
    mMessage = _tcsdup(W2T(text));
    if (checkMsg)
    {
        NS_ENSURE_ARG_POINTER(checkValue);
        mCheckMessage = _tcsdup(W2T(checkMsg));
        mCheckValue = (*checkValue == PR_TRUE) ? TRUE : FALSE;
    }
    if (password)
    {
        mPassword = _tcsdup(W2T(*password));
    }

    mPromptMode = PROMPT_PASS;
    INT result = DialogBoxParam(hInstResource,
        MAKEINTRESOURCE(IDD_PROMPTUSERPASS), hwndParent, PromptProc, (LPARAM) this);

    if (result == IDOK)
    {
        if (password)
        {
            if (*password)
                nsMemory::Free(*password);
            nsAutoString pass(T2W(mPassword));
            *password = ToNewUnicode(pass);
        }

        if (checkValue)
            *checkValue = mCheckValue;

        *_retval = TRUE;
    }
    else
    {
        *_retval = FALSE;
    }

    return NS_OK;
}


BOOL CALLBACK
PromptDlg::PromptProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PromptDlg *pThis = (PromptDlg *) GetWindowLong(hwndDlg, DWL_USER);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        // Initialise pThis
        SetWindowLong(hwndDlg, DWL_USER, lParam);
        pThis = (PromptDlg *) lParam;

        // Set dialog title & message text
        if (pThis->mTitle)
            SetWindowText(hwndDlg, pThis->mTitle);

        SetDlgItemText(hwndDlg, IDC_MESSAGE, pThis->mMessage);

        // Set check message text
        if (pThis->mCheckMessage)
        {
            SetDlgItemText(hwndDlg, IDC_CHECKMSG, pThis->mCheckMessage);
            CheckDlgButton(hwndDlg, IDC_CHECKMSG, pThis->mCheckValue ? BST_CHECKED : BST_UNCHECKED); 
        }
        else
        {
            ShowWindow(GetDlgItem(hwndDlg, IDC_CHECKMSG), SW_HIDE);
        }

        switch (pThis->mPromptMode)
        {
        case PROMPT_PASS:
            EnableWindow(GetDlgItem(hwndDlg, IDC_USERNAME), FALSE);
            if (pThis->mPassword)
            {
                SetDlgItemText(hwndDlg, IDC_PASSWORD, pThis->mPassword);
            }
            break;
        case PROMPT_USERPASS:
            if (pThis->mUsername)
            {
                SetDlgItemText(hwndDlg, IDC_USERNAME, pThis->mUsername);
            }
            if (pThis->mPassword)
            {
                SetDlgItemText(hwndDlg, IDC_PASSWORD, pThis->mPassword);
            }
            break;
        case PROMPT_VALUE:
            if (pThis->mValue)
            {
                SetDlgItemText(hwndDlg, IDC_VALUE, pThis->mValue);
            }
            break;
        }

        return TRUE;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            int id = LOWORD(wParam);
            if (id == IDOK)
            {
                if (pThis->mCheckMessage)
                {
                    pThis->mCheckValue =
                        (IsDlgButtonChecked(hwndDlg, IDC_CHECKMSG) == BST_CHECKED) ?
                            TRUE : FALSE;
                }

                const int bufferSize = 256;
                TCHAR buffer[bufferSize];

                switch (pThis->mPromptMode)
                {
                case PROMPT_USERPASS:
                    if (pThis->mUsername)
                    {
                        free(pThis->mUsername);
                    }
                    memset(buffer, 0, sizeof(buffer));
                    GetDlgItemText(hwndDlg, IDC_USERNAME, buffer, bufferSize);
                    pThis->mUsername = _tcsdup(buffer);

                    // DROP THROUGH !!!!

                case PROMPT_PASS:
                    if (pThis->mPassword)
                    {
                        free(pThis->mPassword);
                    }
                    memset(buffer, 0, sizeof(buffer));
                    GetDlgItemText(hwndDlg, IDC_PASSWORD, buffer, bufferSize);
                    pThis->mPassword = _tcsdup(buffer);
                    break;

                case PROMPT_VALUE:
                    if (pThis->mValue)
                    {
                        free(pThis->mValue);
                    }
                    memset(buffer, 0, sizeof(buffer));
                    GetDlgItemText(hwndDlg, IDC_VALUE, buffer, bufferSize);
                    pThis->mValue = _tcsdup(buffer);
                    break;
                }
                EndDialog(hwndDlg, IDOK);
            }
            else if (id == IDCANCEL)
            {
                EndDialog(hwndDlg, IDCANCEL);
            }
        }
        return TRUE;
    }
    return FALSE;
}

BOOL CALLBACK
PromptDlg::ConfirmProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PromptDlg *pThis = (PromptDlg *) GetWindowLong(hwndDlg, DWL_USER);
    int i;

    switch (uMsg)
    {
    case WM_INITDIALOG:

        // Initialise pThis
        SetWindowLong(hwndDlg, DWL_USER, lParam);
        pThis = (PromptDlg *) lParam;

        // Set dialog title & message text
        SetWindowText(hwndDlg, pThis->mTitle);
        SetDlgItemText(hwndDlg, IDC_MESSAGE, pThis->mMessage);

        // Set check message text
        if (pThis->mCheckMessage)
        {
            SetDlgItemText(hwndDlg, IDC_CHECKMSG, pThis->mCheckMessage);
            CheckDlgButton(hwndDlg, IDC_CHECKMSG, pThis->mCheckValue ? BST_CHECKED : BST_UNCHECKED); 
        }
        else
        {
            ShowWindow(GetDlgItem(hwndDlg, IDC_CHECKMSG), SW_HIDE);
        }

        // Set the button text or hide them
        for (i = 0; i < 3; i++)
        {
            int id = IDC_BUTTON0 + i;
            if (pThis->mButtonTitles[i])
            {
                SetDlgItemText(hwndDlg, id, pThis->mButtonTitles[i]);
            }
            else
            {
                ShowWindow(GetDlgItem(hwndDlg, id), SW_HIDE);
            }
        }
        return TRUE;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            int id = LOWORD(wParam);
            if (id == IDC_BUTTON0 ||
                id == IDC_BUTTON1 ||
                id == IDC_BUTTON2)
            {
                if (pThis->mCheckMessage)
                {
                    pThis->mCheckValue =
                        (IsDlgButtonChecked(hwndDlg, IDC_CHECKMSG) == BST_CHECKED) ?
                            TRUE : FALSE;
                }
                EndDialog(hwndDlg, id - IDC_BUTTON0);
            }
        }
        return FALSE;
    }

    return FALSE;
}


//*****************************************************************************
// CPromptService
//*****************************************************************************   

class CPromptService: public nsIPromptService
{
public:
    CPromptService();
    virtual ~CPromptService();

    CMozillaBrowser *GetOwningBrowser(nsIDOMWindow *parent);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROMPTSERVICE
};

//*****************************************************************************   

NS_IMPL_ISUPPORTS1(CPromptService, nsIPromptService)

CPromptService::CPromptService()
{
}

CPromptService::~CPromptService()
{
}


CMozillaBrowser *CPromptService::GetOwningBrowser(nsIDOMWindow *parent)
{
    if (parent == nsnull)
    {
        // return the first element from the list if there is one
        if (CMozillaBrowser::sBrowserList.Count() > 0)
        {
            return (CMozillaBrowser *) CMozillaBrowser::sBrowserList[0];
        }
        return NULL;
    }

    // Search for the browser with a content window matching the one provided
    PRInt32 i;
    for (i = 0; i < CMozillaBrowser::sBrowserList.Count(); i++)
    {
        CMozillaBrowser *p = (CMozillaBrowser *) CMozillaBrowser::sBrowserList[i];
        if (p->mWebBrowser)
        {
            nsCOMPtr<nsIDOMWindow> domWindow;
            p->mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
            if (domWindow.get() == parent)
            {
                return p;
            }
        }
    }

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// nsIPrompt

NS_IMETHODIMP CPromptService::Alert(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        USES_CONVERSION;
        pOwner->MessageBox(W2T(text), W2T(dialogTitle), MB_OK | MB_ICONEXCLAMATION);
    }
    return NS_OK;
}

NS_IMETHODIMP CPromptService::AlertCheck(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text,
    const PRUnichar *checkMsg, PRBool *checkValue)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        // TODO show dialog with check box
        USES_CONVERSION;
        pOwner->MessageBox(W2T(text), W2T(dialogTitle), MB_OK | MB_ICONEXCLAMATION);
    }
    return NS_OK;
}

NS_IMETHODIMP CPromptService::Confirm(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRBool *_retval)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        USES_CONVERSION;
        int nAnswer = pOwner->MessageBox(W2T(text), W2T(dialogTitle),
            MB_YESNO | MB_ICONQUESTION);
        *_retval = (nAnswer == IDYES) ? PR_TRUE : PR_FALSE;
    }
    return NS_OK;
}


NS_IMETHODIMP CPromptService::ConfirmCheck(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        PromptDlg dlg;
        return dlg.ConfirmCheck(pOwner->m_hWnd, dialogTitle,
            text, checkMsg, checkValue, _retval);
    }
    return NS_OK;
}


NS_IMETHODIMP CPromptService::ConfirmEx(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRUint32 buttonFlags,
    const PRUnichar *button0Title,
    const PRUnichar *button1Title,
    const PRUnichar *button2Title,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRInt32 *buttonPressed)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        PromptDlg dlg;
        return dlg.ConfirmEx(pOwner->m_hWnd, dialogTitle,
            text, buttonFlags, button0Title, button1Title, button2Title,
            checkMsg, checkValue, buttonPressed);
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CPromptService::Prompt(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
    const PRUnichar *text, PRUnichar **value,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        PromptDlg dlg;
        return dlg.Prompt(pOwner->m_hWnd,
            dialogTitle, text, value, checkMsg, checkValue, _retval);
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CPromptService::PromptUsernameAndPassword(nsIDOMWindow *parent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRUnichar **username, PRUnichar **password,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        PromptDlg dlg;
        return dlg.PromptUsernameAndPassword(pOwner->m_hWnd,
            dialogTitle, text, username, password, checkMsg, checkValue, _retval);
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CPromptService::PromptPassword(nsIDOMWindow *parent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text,
    PRUnichar **password,
    const PRUnichar *checkMsg, PRBool *checkValue,
    PRBool *_retval)
{
    CMozillaBrowser *pOwner = GetOwningBrowser(parent);
    if (pOwner)
    {
        PromptDlg dlg;
        return dlg.PromptPassword(pOwner->m_hWnd,
            dialogTitle, text, password, checkMsg, checkValue, _retval);
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CPromptService::Select(nsIDOMWindow *parent,
    const PRUnichar *dialogTitle,
    const PRUnichar *text, PRUint32 count,
    const PRUnichar **selectList, PRInt32 *outSelection,
    PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// CPromptServiceFactory
//*****************************************************************************   

class CPromptServiceFactory : public nsIFactory
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFACTORY

    CPromptServiceFactory();
    virtual ~CPromptServiceFactory();
};

//*****************************************************************************   

NS_IMPL_ISUPPORTS1(CPromptServiceFactory, nsIFactory)

CPromptServiceFactory::CPromptServiceFactory()
{
}

CPromptServiceFactory::~CPromptServiceFactory()
{
}

NS_IMETHODIMP CPromptServiceFactory::CreateInstance(nsISupports *aOuter, const nsIID & aIID, void **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = NULL;  
    CPromptService *inst = new CPromptService;    
    if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = inst->QueryInterface(aIID, aResult);
    if (rv != NS_OK) {  
        // We didn't get the right interface, so clean up  
        delete inst;  
    }  

    return rv;
}

NS_IMETHODIMP CPromptServiceFactory::LockFactory(PRBool lock)
{
    return NS_OK;
}

//*****************************************************************************   

nsresult NS_NewPromptServiceFactory(nsIFactory** aFactory)
{
    NS_ENSURE_ARG_POINTER(aFactory);
    *aFactory = nsnull;

    CPromptServiceFactory *result = new CPromptServiceFactory;
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aFactory = result;

    return NS_OK;
}
