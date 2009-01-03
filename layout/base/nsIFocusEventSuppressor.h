/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsIFocusEventSuppressor_h___
#define nsIFocusEventSuppressor_h___
#include "nsISupports.h"

typedef void (* nsFocusEventSuppressorCallback)(PRBool aSuppress);

#define NS_NSIFOCUSEVENTSUPPRESSORSERVICE_IID \
  { 0x8aae5cee, 0x59ab, 0x42d4, \
    { 0xa3, 0x76, 0xbf, 0x63, 0x54, 0x04, 0xc7, 0x98 } }

class nsIFocusEventSuppressorService : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NSIFOCUSEVENTSUPPRESSORSERVICE_IID)
  virtual void AddObserverCallback(nsFocusEventSuppressorCallback aCallback) = 0;
  virtual void Suppress() = 0;
  virtual void Unsuppress() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFocusEventSuppressorService,
                              NS_NSIFOCUSEVENTSUPPRESSORSERVICE_IID)

#if defined(_IMPL_NS_LAYOUT) || defined(NS_STATIC_FOCUS_SUPPRESSOR)
void NS_SuppressFocusEvent();
void NS_UnsuppressFocusEvent();
void NS_AddFocusSuppressorCallback(nsFocusEventSuppressorCallback aCallback);
void NS_ShutdownFocusSuppressor();
#endif

#define NS_NSIFOCUSEVENTSUPPRESSORSERVICE_CID \
  { 0x35b2656c, 0x4102, 0x4bc1, \
    { 0x87, 0x6a, 0xfd, 0x6c, 0xb8, 0x30, 0x78, 0x7b } }

#define NS_NSIFOCUSEVENTSUPPRESSORSERVICE_CONTRACTID \
  "@mozilla.org/focus-event-suppressor-service;1"

#endif
