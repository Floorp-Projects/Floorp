/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *  Patrick C. Beard <beard@netscape.com>
 */

#ifndef nsDeviceContextSpecX_h___
#define nsDeviceContextSpecX_h___

#include "nsIDeviceContextSpec.h"
#include "nsIPrintingContext.h"
#include "nsDeviceContextMac.h"
#include <PMApplication.h>

class nsDeviceContextSpecX : public nsIDeviceContextSpec, public nsIPrintingContext {
public:
    /**
     * Construct a nsDeviceContextSpecMac, which is an object which contains and manages a mac printrecord
     * @update  dc 12/02/98
     */
    nsDeviceContextSpecX();

    NS_DECL_ISUPPORTS

    /**
     * Initialize the nsDeviceContextSpecMac for use.  This will allocate a printrecord for use
     * @update   dc 12/02/98
     * @param aQuiet if PR_TRUE, prevent the need for user intervention
     *        in obtaining device context spec. if nsnull is passed in for
     *        the aOldSpec, this will typically result in getting a device
     *        context spec for the default output device (i.e. default
     *        printer).
     * @return error status
     */
    NS_IMETHOD Init(PRBool aQuiet);

    /**
     * This will tell if the printmanager is currently open
     * @update   dc 12/03/98
     * @param aIsOpen True or False depending if the printmanager is open
     * @return error status
     */
    NS_IMETHOD PrintManagerOpen(PRBool* aIsOpen);

    /**
     * Closes the printmanager if it is open.
     * @update   dc 12/03/98
     * @return error status
     */
    NS_IMETHOD ClosePrintManager();

    NS_IMETHOD BeginDocument();
    
    NS_IMETHOD EndDocument();
    
    NS_IMETHOD BeginPage();
    
    NS_IMETHOD EndPage();

    NS_IMETHOD GetPrinterResolution(double* aResolution);
    
    NS_IMETHOD GetPageRect(double* aTop, double* aLeft, double* aBottom, double* aRight);
protected:
/**
 * Destuct a nsDeviceContextSpecMac, this will release the printrecord
 * @update  dc 12/02/98
 */
  virtual ~nsDeviceContextSpecX();

protected:
    PMPrintSession mPrintSession;               // printing session.
    PMPageFormat mPageFormat;                   // page format.
    PMPrintSettings mPrintSettings;             // print settings.
    CGrafPtr mSavedPort;                        // saved graphics port.
};

#endif
