/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __CHBrowserService_h__
#define __CHBrowserService_h__

#import "nsAlertController.h"
#include "nsCOMPtr.h"
#include "nsIWindowCreator.h"
#include "nsIHelperAppLauncherDialog.h"
#include "nsIFactory.h"

// two shutdown notifcations exist to allow listeners to guarantee ordering of
// notifcations, such that they can save state before xpcom-reliant data structures
// are torn down.
extern NSString* TermEmbeddingNotificationName;   // someone called TermEmbedding
extern NSString* XPCOMShutDownNotificationName;   // XPCOM is about to shut down

class nsModuleComponentInfo;

class CHBrowserService :  public nsIWindowCreator,
                          public nsIFactory, 
                          public nsIHelperAppLauncherDialog
{
public:
  CHBrowserService();
  virtual ~CHBrowserService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWCREATOR
  NS_DECL_NSIFACTORY
  NS_DECL_NSIHELPERAPPLAUNCHERDIALOG
  
  static nsresult InitEmbedding();
  static void TermEmbedding();
  static void BrowserClosed();

  // Call to override Gecko components with ones implemented by the
  // embedding application. Some examples are security dialogs, password
  // manager, and Necko prompts. This can be called at any time after
  // XPCOM has been initialized.
  static void RegisterAppComponents(const nsModuleComponentInfo* inComponents, const int inNumComponents);
  
  static void SetAlertController(nsAlertController* aController);
  static nsAlertController* GetAlertController();

public:
  static PRUint32 sNumBrowsers;

private:
  static void ShutDown();

  static CHBrowserService* sSingleton;
  static nsAlertController* sController;
  static PRBool sCanTerminate;
};


#endif // __CHBrowserService_h__

