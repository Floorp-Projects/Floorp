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
 * The Original Code is developed for mozilla.
 *
 * The Initial Developer of the Original Code is
 * Kenneth Herron <kherron@fastmail.us>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@nrubsig.org>
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
 
#ifndef NSEPSOBJECTPS_H
#define NSEPSOBJECTPS_H

#include <stdio.h>

#include "nscore.h"
#include "prtypes.h"
#include "nsString.h"

class nsEPSObjectPS {
    public:
        /** ---------------------------------------------------
         * Constructor
         */
        nsEPSObjectPS(FILE *aFile);

        /** ---------------------------------------------------
         * @return the result code from parsing the EPS data.
         * If the return value is not NS_OK, the EPS object is
         * invalid and should not be used further.
         */
        nsresult GetStatus() { return mStatus; };

        /** ---------------------------------------------------
         * Return Bounding box coordinates: lower left x,
         * lower left y, upper right x, upper right y.
         */
        PRFloat64 GetBoundingBoxLLX() { return mBBllx; };
        PRFloat64 GetBoundingBoxLLY() { return mBBlly; };
        PRFloat64 GetBoundingBoxURX() { return mBBurx; };
        PRFloat64 GetBoundingBoxURY() { return mBBury; };

        /** ---------------------------------------------------
         * Write the EPS object to the provided file stream.
         * @return NS_OK if the EPS was written successfully, or
         *         a suitable error code.
         */
        nsresult WriteTo(FILE *aDest);

    private:
        nsresult        mStatus;
        FILE *          mEPSF;
        PRFloat64       mBBllx,
                        mBBlly,
                        mBBurx,
                        mBBury;

        void            Parse();
        PRBool          EPSFFgets(nsACString& aBuffer);
};

#endif /* !NSEPSOBJECTPS_H */

