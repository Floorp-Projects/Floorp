/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   
 */

#ifndef nsPrintSettingsMac_h__
#define nsPrintSettingsMac_h__

#include "nsPrintSettingsImpl.h"  
#include "nsIPrintSettingsMac.h"  
#include <Printing.h>


//*****************************************************************************
//***    nsPrintSettingsMac
//*****************************************************************************
#if !TARGET_CARBON

class nsPrintSettingsMac : public nsPrintSettings,
                           public nsIPrintSettingsMac
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRINTSETTINGSMAC

  nsPrintSettingsMac();
  virtual ~nsPrintSettingsMac();
  
  nsresult Init();

protected:
  nsPrintSettingsMac(const nsPrintSettingsMac& src);
  nsPrintSettingsMac& operator=(const nsPrintSettingsMac& rhs);

  nsresult _Clone(nsIPrintSettings **_retval);
  nsresult _Assign(nsIPrintSettings *aPS);

  THPrint mPrintRecord;

};

#endif /* TARGET_CARBON */

#endif /* nsPrintSettingsMac_h__ */
