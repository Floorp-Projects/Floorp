/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marria Nazif <marria@gmail.com>
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

#ifndef nsUICommandCollector_h_
#define nsUICommandCollector_h_

#include "nsIObserver.h"
#include "nsIDOMEventListener.h"
#include "nsIMetricsCollector.h"

#include "nsDataHashtable.h"

class nsIDOMWindow;

class nsUICommandCollector : public nsIObserver,
                             public nsIDOMEventListener,
                             public nsIMetricsCollector
{
 public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIMETRICSCOLLECTOR
  
  static PLDHashOperator PR_CALLBACK AddCommandEventListener(
    const nsIDOMWindow* key, PRUint32 windowID, void* userArg);

  static PLDHashOperator PR_CALLBACK RemoveCommandEventListener(
    const nsIDOMWindow* key, PRUint32 windowID, void* userArg);

  nsUICommandCollector();

 private:
  ~nsUICommandCollector();
};

#define NS_UICOMMANDCOLLECTOR_CLASSNAME "UI Command Collector"
#define NS_UICOMMANDCOLLECTOR_CID \
{ 0xcc2fedc9, 0x8b2e, 0x4e2c, {0x97, 0x07, 0xe2, 0xe5, 0x6b, 0xeb, 0x01, 0x85}}

#endif // nsUICommandCollector_h_
