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
 * Ken Herron <kherron@fastmail.us>.
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


#include "nscore.h"
#include "nsIDeviceContext.h"   // NS_ERROR_GFX_*
#include "nsIDeviceContextPS.h" // NS_POSTSCRIPT_DRIVER_NAME_LEN
#include "nsIDeviceContextSpecPS.h"
#include "nsPrintJobPS.h"
#include "nsReadableUtils.h"

#include "prenv.h"
#include "prinit.h"
#include "prlock.h"
#include "prprf.h"

#include <stdlib.h>
#include <sys/wait.h>


/* Routines to set environment variables. These are defined toward
 * the end of this file.
 */
static PRStatus EnvLock();
static PRStatus EnvSetPrinter(nsCString&);
static void EnvClear();


/* ~nsIPrintJobPS() is virtual, so must implement a destructor. */
nsIPrintJobPS::~nsIPrintJobPS() {}

/**** nsPrintJobPreviewPS - Stub class for print preview ****/
nsresult
nsPrintJobPreviewPS::Init(nsIDeviceContextSpecPS *aSpec)
{
    return NS_OK;
}


/**** nsPrintJobFilePS - Print-to-file support ****/

/* Print-to-file constructor */
nsPrintJobFilePS::nsPrintJobFilePS() : mDestHandle(nsnull) { }

/* Print-to-file destructor */
nsPrintJobFilePS::~nsPrintJobFilePS()
{
    if (mDestHandle)
        fclose(mDestHandle);
}

/**
 * Initialize the print-to-file object from the printing spec.
 * See nsPrintJobPS.h for details.
 */
nsresult
nsPrintJobFilePS::Init(nsIDeviceContextSpecPS *aSpec)
{
    NS_PRECONDITION(aSpec, "aSpec must not be NULL");
#ifdef DEBUG
    PRBool toPrinter;
    aSpec->GetToPrinter(toPrinter);
    NS_PRECONDITION(!toPrinter, "This print job is to a printer");
#endif
    const char *path;
    aSpec->GetPath(&path);
    mDestination = path;
    return NS_OK;
}


/**
 * Create the final output file and copy the temporary files there.
 * See nsIPrintJobPS.h and nsPrintJobPS.h for details.
 */
nsresult
nsPrintJobFilePS::StartSubmission(FILE **aHandle)
{
    NS_PRECONDITION(aHandle, "aHandle is NULL");
    NS_PRECONDITION(!mDestination.IsEmpty(), "No destination");
    NS_PRECONDITION(!mDestHandle, "Already have a destination handle");

    nsCOMPtr<nsILocalFile> destFile;
    nsresult rv = NS_NewNativeLocalFile(GetDestination(),
            PR_FALSE, getter_AddRefs(destFile));
    if (NS_SUCCEEDED(rv))
        rv = destFile->OpenANSIFileDesc("w", &mDestHandle);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE);
    NS_POSTCONDITION(mDestHandle,
            "OpenANSIFileDesc succeeded but no file handle");
    *aHandle = mDestHandle;
    return rv;
}


/**
 * Finish a print job. See nsIPrintJobPS.h and nsPrintJobPS.h for details.
 */
nsresult
nsPrintJobFilePS::FinishSubmission()
{
    NS_PRECONDITION(mDestHandle, "No destination file handle");

    fclose(mDestHandle);
    mDestHandle = nsnull;
    return NS_OK;
}


#ifdef VMS

/**** Print-to-command on VMS. ****/

/* This implementation writes the print job to a temporary file, then runs
 * the print command with the name of that file appended.
 */


/**
 * Initialize a VMS print-to-command object from the printing spec.
 * See nsIPrintJobPS.h and nsPrintJobPS.h.
 */
nsresult
nsPrintJobVMSCmdPS::Init(nsIDeviceContextSpecPS *aSpec)
{
    NS_PRECONDITION(aSpec, "argument must not be NULL");
#ifdef DEBUG
    PRBool toPrinter;
    aSpec->GetToPrinter(toPrinter);
    NS_PRECONDITION(toPrinter, "This print job is not to a printer");
#endif

    /* Print command. This is stored as the destination string. */
    const char *command;
    aSpec->GetCommand(&command);
    SetDestination(command);

    /* Printer name */
    const char *printerName;
    aSpec->GetPrinterName(&printerName);
    if (printerName) {
        printerName += NS_POSTSCRIPT_DRIVER_NAME_LEN;
        if (0 != strcmp(printerName, "default"))
            mPrinterName = printerName;
    }
    return NS_OK;
}


/**
 * Create the temporary file for the print job and return a file handle
 * to the caller.
 * See nsIPrintJobPS.h and nsPrintJobPS.h for details.
 */
nsresult
nsPrintJobVMSCmdPS::StartSubmission(FILE **aHandle)
{
    NS_PRECONDITION(aHandle, "aHandle is NULL");
    NS_PRECONDITION(!GetDestination().IsEmpty(), "No destination");
    NS_PRECONDITION(!GetDestHandle(), "Already have a destination handle");

    /* Create the final output file */
    FILE *printHandle = nsnull;
    nsresult rv = mTempFactory.CreateTempFile(
            getter_AddRefs(mTempFile), &printHandle, "w+");
    if (NS_SUCCEEDED(rv)) {
        SetDestHandle(printHandle);
        *aHandle = printHandle;
    }
    return rv;
}

nsresult
nsPrintJobVMSCmdPS::FinishSubmission()
{
    NS_PRECONDITION(GetDestHandle(), "No destination file handle");
    NS_PRECONDITION(!GetDestination().IsEmpty(), "No destination");

    /* Close the temporary file handle */
    fclose(GetDestHandle());
    SetDestHandle(nsnull);

    /* construct the print command */
    nsCAutoString printFileName;
    nsresult rv = mTempFile->GetNativePath(printFileName);
    if (NS_SUCCEEDED(rv)) {
        nsCAutoString cmd(GetDestination());
        cmd += " "; cmd += printFileName; cmd += ".";

        /* Set up the environment. */
        if (PR_SUCCESS != EnvLock())
            return NS_ERROR_OUT_OF_MEMORY;
        if (!mPrinterName.IsEmpty())
            EnvSetPrinter(mPrinterName);

        /* Run the print command */
        int presult = system(cmd.get());

        /* Clean up */
        EnvClear();
        mTempFile->Remove(PR_FALSE);

        rv = (!WIFEXITED(presult) || (EXIT_SUCCESS != WEXITSTATUS(presult)))
            ? NS_ERROR_GFX_PRINTER_CMD_FAILURE : NS_OK;
    }
    return rv;
}


#else   /* NOT VMS */

/**** Print-to-Pipe for unix and unix-like systems ****/

/* This launches a command using popen(); the print job is then written
 * to the pipe.
 */

/* Destructor. We must override the print-to-file destructor in order
 * to pclose() any open file handle.
 */
nsPrintJobPipePS::~nsPrintJobPipePS()
{
    if (GetDestHandle()) {
        pclose(GetDestHandle());
        SetDestHandle(nsnull);
    }
}


/**
 * Initialize a print-to-pipe object.
 * See nsIPrintJobPS.h and nsPrintJobPS.h for details.
 */
nsresult
nsPrintJobPipePS::Init(nsIDeviceContextSpecPS *aSpec)
{
    NS_PRECONDITION(aSpec, "argument must not be NULL");
#ifdef DEBUG
    PRBool toPrinter;
    aSpec->GetToPrinter(toPrinter);
    NS_PRECONDITION(toPrinter, "Wrong class for this print job");
#endif

    /* Print command. This is stored as the destination string. */
    const char *command;
    aSpec->GetCommand(&command);
    SetDestination(command);

    /* Printer name */
    const char *printerName;
    aSpec->GetPrinterName(&printerName);
    if (printerName) {
        printerName += NS_POSTSCRIPT_DRIVER_NAME_LEN;
        if (0 != strcmp(printerName, "default"))
            mPrinterName = printerName;
    }
    return NS_OK;
}


/**
 * Launch the print command using popen(), then copy the print job data
 * to the pipe. See nsIPrintJobPS.h and nsPrintJobPS.h for details.
 */
nsresult
nsPrintJobPipePS::StartSubmission(FILE **aHandle)
{
    NS_PRECONDITION(aHandle, "aHandle is NULL");
    NS_PRECONDITION(!GetDestination().IsEmpty(), "No destination");
    NS_PRECONDITION(!GetDestHandle(), "Already have a destination handle");

    if (PR_SUCCESS != EnvLock())
        return NS_ERROR_OUT_OF_MEMORY;  // Couldn't allocate the object?
    if (!mPrinterName.IsEmpty())
        EnvSetPrinter(mPrinterName);

    FILE *destPipe = popen(GetDestination().get(), "w");
    EnvClear();
    if (!destPipe)
        return NS_ERROR_GFX_PRINTER_CMD_FAILURE;
    SetDestHandle(destPipe);
    *aHandle = destPipe;
    return NS_OK;
}

nsresult
nsPrintJobPipePS::FinishSubmission()
{
    NS_PRECONDITION(GetDestHandle(), "No destination file handle");
    NS_PRECONDITION(!GetDestination().IsEmpty(), "No destination");

    int presult = pclose(GetDestHandle());
    SetDestHandle(nsnull);
    if (!WIFEXITED(presult) || (EXIT_SUCCESS != WEXITSTATUS(presult)))
        return NS_ERROR_GFX_PRINTER_CMD_FAILURE;
    return NS_OK;
}


#endif  /* VMS */


/* Routines to set the MOZ_PRINTER_NAME environment variable and to
 * single-thread print jobs while the variable is set.
 */

static PRLock *EnvLockObj;
static PRCallOnceType EnvLockOnce;

/* EnvLock callback function */
static PRStatus
EnvLockInit()
{
    EnvLockObj = PR_NewLock();
    return EnvLockObj ? PR_SUCCESS : PR_FAILURE;
}


/**
 * Get the lock for setting printing-related environment variables and
 * running print commands.
 * @return PR_SUCCESS on success
 *         PR_FAILURE if the lock object could not be initialized.
 *                 
 */
static PRStatus
EnvLock()
{
    if (PR_FAILURE == PR_CallOnce(&EnvLockOnce, EnvLockInit))
        return PR_FAILURE;
    PR_Lock(EnvLockObj);
    return PR_SUCCESS;
}


static char *EnvPrinterString;
static const char EnvPrinterName[] = { "MOZ_PRINTER_NAME" };


/**
 * Set MOZ_PRINTER_NAME to the specified string.
 * @param aPrinter The value for MOZ_PRINTER_NAME. May be an empty string.
 * @return PR_SUCCESS on success.
 *         PR_FAILURE if memory could not be allocated.
 */
static PRStatus
EnvSetPrinter(nsCString& aPrinter)
{
    /* Construct the new environment string */
    char *newVar = PR_smprintf("%s=%s", EnvPrinterName, aPrinter.get());
    if (!newVar)
        return PR_FAILURE;

    /* Add it to the environment and dispose of any old string */
    PR_SetEnv(newVar);
    if (EnvPrinterString)
        PR_smprintf_free(EnvPrinterString);
    EnvPrinterString = newVar;

    return PR_SUCCESS;
}


/**
 * Clear the printer environment variable and release the environment lock.
 */
static void
EnvClear()
{
    if (EnvPrinterString) {
        /* On some systems, setenv("FOO") will remove FOO
         * from the environment.
         */
        PR_SetEnv(EnvPrinterName);
        if (!PR_GetEnv(EnvPrinterName)) {
            /* It must have worked */
            PR_smprintf_free(EnvPrinterString);
            EnvPrinterString = nsnull;
        }
    }
    PR_Unlock(EnvLockObj);
}
