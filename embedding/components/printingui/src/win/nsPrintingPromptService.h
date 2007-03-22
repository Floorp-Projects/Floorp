/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __nsPrintingPromptService_h
#define __nsPrintingPromptService_h

#include <windows.h>

// {E042570C-62DE-4bb6-A6E0-798E3C07B4DF}
#define NS_PRINTINGPROMPTSERVICE_CID \
 {0xe042570c, 0x62de, 0x4bb6, { 0xa6, 0xe0, 0x79, 0x8e, 0x3c, 0x7, 0xb4, 0xdf}}
#define NS_PRINTINGPROMPTSERVICE_CONTRACTID \
 "@mozilla.org/embedcomp/printingprompt-service;1"

#include "nsCOMPtr.h"
#include "nsIPrintingPromptService.h"
#include "nsPIPromptService.h"
#include "nsIWindowWatcher.h"

// Printing Progress Includes
#include "nsPrintProgress.h"
#include "nsPrintProgressParams.h"
#include "nsIWebProgressListener.h"

class nsIDOMWindow;
class nsIDialogParamBlock;

class nsPrintingPromptService: public nsIPrintingPromptService,
                               public nsIWebProgressListener
{
public:
    nsPrintingPromptService();
    virtual ~nsPrintingPromptService();

    nsresult Init();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRINTINGPROMPTSERVICE
    NS_DECL_NSIWEBPROGRESSLISTENER

private:
    HWND GetHWNDForDOMWindow(nsIDOMWindow *parent);
    nsresult DoDialog(nsIDOMWindow *aParent,
                      nsIDialogParamBlock *aParamBlock, 
                      nsIPrintSettings* aPS,
                      const char *aChromeURL);

    nsCOMPtr<nsIWindowWatcher> mWatcher;
    nsCOMPtr<nsIPrintProgress> mPrintProgress;
    nsCOMPtr<nsIWebProgressListener> mWebProgressListener;

};

#endif

