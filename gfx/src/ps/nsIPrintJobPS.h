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

#ifndef nsIPrintJobPS_h__
#define nsIPrintJobPS_h__

#include <stdio.h>

class nsIDeviceContextSpecPS;

/*
 * This is an interface for a class that accepts and submits print jobs.
 *
 * Instances should be obtained through nsPrintJobFactoryPS::CreatePrintJob().
 * After obtaining a print job object, the caller can retrieve the PostScript
 * object associated with the print job and use that to generate the content.
 * Once that is done, the caller may call Submit() to finalize and print
 * the job, or Cancel() to abort the job.
 */

class nsIPrintJobPS
{
public:
    /**
     * Obligatory virtual destructor for polymorphic objects.
     */
    virtual ~nsIPrintJobPS();

    /* Allow the print job factory to create instances */
    friend class nsPrintJobFactoryPS;

    /**
     * Begin submitting a print job. 
     * @param aHandle If the return value is NS_OK, this will be filled
     *                in with a file handle which the caller should use
     *                to write the text of the print job. The file
     *                handle may not support seeking. The caller must
     *                not close the file handle.
     * @return NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED if the print
     *               job object doesn't actually support printing (e.g.
     *               for print preview)
     *         NS_OK for success
     *         Another value for initialization/startup failures.
     */
    virtual nsresult StartSubmission(FILE **aHandle) = 0;

    /**
     * Finish submitting a print job. The caller must call this after
     * calling StartSubmission() and writing the text of the print job
     * to the file handle. The return value indicates the overall success
     * or failure of the print operation.
     *
     * @return NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED if the print
     *               job object doesn't actually support printing (e.g.
     *               for print preview)
     *         NS_OK for success
     *         Another value for initialization/startup failures.
     */
    virtual nsresult FinishSubmission() = 0;

protected:
    /**
     * Initialize an object from a device context spec. This must be
     * called before any of the public methods.
     * @param aContext The device context spec describing the
     *                 desired print job.
     * @return NS_OK or a suitable error value.
     */
    virtual nsresult Init(nsIDeviceContextSpecPS *aContext) = 0;
};


#endif /* nsIPrintJobPS_h__ */
