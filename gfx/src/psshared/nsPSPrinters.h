/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is the Mozilla PostScript driver printer list component.
 *
 * The Initial Developer of the Original Code is
 * Kenneth Herron <kherron+mozilla@fmailbox.com>
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

#ifndef nsPSPrinters_h___
#define nsPSPrinters_h___

#include "nsString.h"
#include "nsVoidArray.h"
#include "prtypes.h"
#include "nsCUPSShim.h"

class nsIPrefService;
class nsIPrefBranch;
class nsCUPSShim;

class nsPSPrinterList {
    public:
        /**
         * Initialize a printer manager object.
         * @return NS_ERROR_NOT_INITIALIZED if unable to access prefs
         *         NS_OK for successful initialization.
         */
        nsresult Init();

        /**
         * Is the PostScript module enabled or disabled?
         * @return PR_TRUE if enabled,
         *         PR_FALSE if not.
         */
        PRBool Enabled();

        /**
         * Obtain a list of printers (print destinations) supported by the
         * PostScript module, Each entry will be in the form <type>/<name>,
         * where <type> is a printer type string, and <name> is the actual
         * printer name.
         *
         * @param aList Upon return, this is populated with the list of
         *              printer names as described above, replacing any
         *              previous contents. Each entry is a UTF8 string.
         *              There should always be at least one entry. The
         *              first entry is the default print destination.
         */
        void GetPrinterList(nsCStringArray& aList);

        enum PrinterType {
            kTypeUnknown,         // Not actually handled by the PS module
            kTypePS,              // Generic postscript module printer
            kTypeCUPS,            // CUPS printer
        };

        /**
         * Identify a printer's type from its name.
         * @param aName The printer's full name as a UTF8 string, including
         *              the <type> portion as described for GetPrinterList().
         * @return The PrinterType value for this name.
         */
        static PrinterType GetPrinterType(const nsACString& aName);

    private:
        nsCOMPtr<nsIPrefService> mPrefSvc;
        nsCOMPtr<nsIPrefBranch> mPref;
        nsCUPSShim mCups;
};

#endif /* nsPSPrinters_h___ */
