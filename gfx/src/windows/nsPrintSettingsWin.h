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

#ifndef nsPrintSettingsWin_h__
#define nsPrintSettingsWin_h__

#include "nsPrintSettingsImpl.h"  
#include "nsIPrintSettingsWin.h"  
#include <windows.h>  


//*****************************************************************************
//***    nsPrintSettingsWin
//*****************************************************************************
class nsPrintSettingsWin : public nsPrintSettings,
                                  public nsIPrintSettingsWin
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRINTSETTINGSWIN

  nsPrintSettingsWin();
  nsPrintSettingsWin(const nsPrintSettingsWin& aPS);
  virtual ~nsPrintSettingsWin();

  /**
   * Makes a new copy
   */
  virtual nsresult _Clone(nsIPrintSettings **_retval);

  /**
   * Assigns values
   */
  virtual nsresult _Assign(nsIPrintSettings* aPS);

  /**
   * Assignment
   */
  nsPrintSettingsWin& operator=(const nsPrintSettingsWin& rhs);

protected:
  void CopyDevMode(DEVMODE* aInDevMode, DEVMODE *& aOutDevMode);

  char*     mDeviceName;
  char*     mDriverName;
  LPDEVMODE mDevMode;
};



#endif /* nsPrintSettingsWin_h__ */
