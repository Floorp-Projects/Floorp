/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *		John C. Griggs <johng@corel.com>
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
 
    NS_IMETHOD GetLandscape(PRBool &aLandscape);

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
