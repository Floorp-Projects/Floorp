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

#ifndef nsDeviceContextSpecQT_h___
#define nsDeviceContextSpecQT_h___

#include "nsIDeviceContextSpec.h"
#include "nsDeviceContextSpecQT.h"
#include "nsIDeviceContextSpecPS.h"

#include "../gtk/nsPrintdGTK.h"

class nsDeviceContextSpecQT : public nsIDeviceContextSpec, 
                              public nsIDeviceContextSpecPS
{
public:
/**
 * Construct a nsDeviceContextSpecQT, which is an object which contains and manages a printrecord
 * @update  dc 12/02/98
 */
    nsDeviceContextSpecQT();

    NS_DECL_ISUPPORTS

/**
 * Initialize the nsDeviceContextSpecQT for use.  This will allocate a printrecord for use
 * @update   dc 2/16/98
 * @param aQuiet if PR_TRUE, prevent the need for user intervention
 *        in obtaining device context spec. if nsnull is passed in for
 *        the aOldSpec, this will typically result in getting a device
 *        context spec for the default output device (i.e. default
 *        printer).
 * @return error status
 */
    NS_IMETHOD Init(PRBool aQuiet);
  
  
/**
 * Closes the printmanager if it is open.
 * @update   dc 2/13/98
 * @return error status
 */
    NS_IMETHOD ClosePrintManager();

    NS_IMETHOD GetToPrinter(PRBool &aToPrinter);
 
    NS_IMETHOD GetFirstPageFirst(PRBool &aFpf);
 
    NS_IMETHOD GetGrayscale(PRBool &aGrayscale);
 
    NS_IMETHOD GetSize(int &aSize);
 
    NS_IMETHOD GetTopMargin(float &value);
 
    NS_IMETHOD GetBottomMargin(float &value);
 
    NS_IMETHOD GetLeftMargin(float &value);
 
    NS_IMETHOD GetRightMargin(float &value);
 
    NS_IMETHOD GetCommand(char **aCommand);
 
    NS_IMETHOD GetPath(char **aPath);
 
    NS_IMETHOD GetPageDimensions(float &aWidth, float &aHeight);
 
    NS_IMETHOD GetUserCancelled(PRBool &aCancel);
 
protected:
/**
 * Destuct a nsDeviceContextSpecQT, this will release the printrecord
 * @update  dc 2/16/98
 */
    virtual ~nsDeviceContextSpecQT();

protected:
    UnixPrData mPrData;	
};

#endif
