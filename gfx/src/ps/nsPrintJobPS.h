/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Ken Herron <kherron@fastmail.us>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsPrintJobPS_h__
#define nsPrintJobPS_h__

#include "nsDebug.h"
#include "nsIDeviceContext.h"   // for NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED
#include "nsILocalFile.h"
#include "nsIPrintJobPS.h"
#include "nsString.h"
#include "nsTempfilePS.h"


/* Print job class for print preview operations. */

class nsPrintJobPreviewPS : public nsIPrintJobPS {
    public:
        /* see nsIPrintJobPS.h. Print preview doesn't actually
         * implement printing.
         */
        nsresult StartSubmission(FILE **aHandle)
            { return NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED; }

        nsresult FinishSubmission()
            { return NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED; }


    protected:
        /* See nsIPrintJobPS.h. */
        nsresult Init(nsIDeviceContextSpecPS *);
};


/* Print job class for print-to-file. */
class nsPrintJobFilePS : public nsIPrintJobPS {
    public:
        nsPrintJobFilePS();
        ~nsPrintJobFilePS();

        /* see nsIPrintJobPS.h */
        nsresult StartSubmission(FILE **aHandle);
        nsresult FinishSubmission();

    protected:
        /* see nsIPrintJobPS.h */
        nsresult Init(nsIDeviceContextSpecPS *);

        /**
         * Set the destination file handle.
         * @param aHandle New value for the handle.
         */
        void SetDestHandle(FILE *aHandle) { mDestHandle = aHandle; }

        /**
         * Get the current value for the destination file handle.
         * @return the current value for the destination file handle.
         */
        FILE *GetDestHandle() { return mDestHandle; }

        /**
         * Set a string representing the destination. For print-to-file
         * this is the name of the destination file. Subclasses could
         * store something else here.
         * @param aDest Destination filename.
         */
        void SetDestination(const char *aDest) { mDestination = aDest; }

        /**
         * Get the string representing the destination.
         * @return The current value of the destination string.
         */
        nsCString& GetDestination() { return mDestination; }


    private:
        FILE *mDestHandle;                  // Destination file handle
        nsCString mDestination;
};

#ifdef VMS

/* This is the class for printing by launching a command on VMS. The
 * string for GetDestination() and SetDestination() is a command to be
 * executed, rather than a filename.
 */
class nsPrintJobVMSCmdPS : public nsPrintJobFilePS {
    public:
        /* see nsIPrintJobPS.h */
        nsresult StartSubmission(FILE **aHandle);
        nsresult FinishSubmission();

    protected:
        nsresult Init(nsIDeviceContextSpecPS *);

    private:
        nsCString mPrinterName;
        nsTempfilePS mTempFactory;
        nsCOMPtr<nsILocalFile> mTempFile;
};

#else   /* Not VMS */

/* This is the class for printing to a pipe. The destination for
 * GetDestination() and SetDestination() is a command to be executed,
 * rather than a filename.
 */
class nsPrintJobPipePS : public nsPrintJobFilePS {
    public:
        /* see nsIPrintJobPS.h */
        ~nsPrintJobPipePS();
        nsresult StartSubmission(FILE **aHandle);
        nsresult FinishSubmission();

    protected:
        nsresult Init(nsIDeviceContextSpecPS *);

    private:
        nsCString mPrinterName;
};

#endif  /* VMS */

#endif /* nsPrintJobPS_h__ */
