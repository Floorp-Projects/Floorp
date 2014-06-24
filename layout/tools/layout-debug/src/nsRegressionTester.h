/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRegressionTester_h__
#define nsRegressionTester_h__

#include "nsCOMPtr.h"

#include "nsILayoutRegressionTester.h"  
#include "nsILayoutDebugger.h"

class nsIDOMWindow;
class nsIDocShell;

//*****************************************************************************
//***    nsRegressionTester
//*****************************************************************************
class  nsRegressionTester : public nsILayoutRegressionTester
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILAYOUTREGRESSIONTESTER

  nsRegressionTester();

protected:
  virtual ~nsRegressionTester();
  nsresult    GetDocShellFromWindow(nsIDOMWindow* inWindow, nsIDocShell** outShell);
};



#endif /* nsRegressionTester_h__ */
