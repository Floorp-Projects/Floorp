/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Kenneth Herron <kherron@fastmail.us>.
 * Portions created by the Initial Developer are Copyright (C) 2004
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


#include "nsDebug.h"
#include "nsPrintJobFactoryPS.h"
#include "nsIDeviceContextSpecPS.h"
#include "nsPrintJobPS.h"

/**
 * Construct a print job object for the given device context spec.
 *
 * @param aSpec     An nsIDeviceContextSpecPS object for the print
 *                  job in question.
 * @param aPrintJob A pointer to a print job object which will
 *                  handle the print job.
 * @return NS_OK if all is well, or a suitable error value.
 */

nsresult
nsPrintJobFactoryPS::CreatePrintJob(nsIDeviceContextSpecPS *aSpec,
        nsIPrintJobPS* &aPrintJob)
{
    NS_PRECONDITION(nsnull != aSpec, "aSpec is NULL");

    nsIPrintJobPS *newPJ;

    PRBool setting;
    aSpec->GetIsPrintPreview(setting);
    if (setting)
        newPJ = new nsPrintJobPreviewPS();
    else {
        aSpec->GetToPrinter(setting);
        if (setting)
#ifdef VMS
            newPJ = new nsPrintJobVMSCmdPS();
#else
            newPJ = new nsPrintJobPipePS();
#endif
        else
            newPJ = new nsPrintJobFilePS();
    }
    if (!newPJ)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = newPJ->Init(aSpec);
    if (NS_FAILED(rv))
        delete newPJ;
    else
        aPrintJob = newPJ;
    return rv;
}
