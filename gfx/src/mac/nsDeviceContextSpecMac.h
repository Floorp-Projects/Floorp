/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsDeviceContextSpecMac_h___
#define nsDeviceContextSpecMac_h___

#include "nsIDeviceContextSpec.h"
#include "nsDeviceContextMac.h"
#include <Printing.h>

class nsDeviceContextSpecMac : public nsIDeviceContextSpec
{
public:
/**
 * Construct a nsDeviceContextSpecMac, which is an object which contains and manages a mac printrecord
 * @update  dc 12/02/98
 */
  nsDeviceContextSpecMac();

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
  NS_IMETHOD Init(PRBool	aQuiet);
  
/**
 * This will tell if the printmanager is currently open
 * @update   dc 12/03/98
 * @param aIsOpen True or False depending if the printmanager is open
 * @return error status
 */
  NS_IMETHOD PrintManagerOpen(PRBool* aIsOpen) {*aIsOpen = mPrintManagerOpen; return NS_OK;}
  
/**
 * Closes the printmanager if it is open.
 * @update   dc 12/03/98
 * @return error status
 */
  NS_IMETHOD ClosePrintManager();

protected:
/**
 * Destuct a nsDeviceContextSpecMac, this will release the printrecord
 * @update  dc 12/02/98
 */
  virtual ~nsDeviceContextSpecMac();

protected:
	THPrint		mPrtRec;									// the print record
	PRBool		mPrintManagerOpen;				// tells if the print manager is open
	TPrPort*	mPrinterPort;
	
friend nsDeviceContextMac;
};

#endif
