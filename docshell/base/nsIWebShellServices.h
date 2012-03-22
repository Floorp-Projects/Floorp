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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsIWebShellServices_h___
#define nsIWebShellServices_h___

#include "nsISupports.h"
#include "nsCharsetSource.h"

// Interface ID for nsIWebShellServices

/* 0c628af0-5638-4703-8f99-ed6134c9de18 */
#define NS_IWEB_SHELL_SERVICES_IID \
{ 0x0c628af0, 0x5638, 0x4703, {0x8f, 0x99, 0xed, 0x61, 0x34, 0xc9, 0xde, 0x18} }

//----------------------------------------------------------------------

class nsIWebShellServices : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWEB_SHELL_SERVICES_IID)

  NS_IMETHOD ReloadDocument(const char* aCharset = nsnull , 
                            PRInt32 aSource = kCharsetUninitialized) = 0;
  NS_IMETHOD StopDocumentLoad(void) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIWebShellServices, NS_IWEB_SHELL_SERVICES_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIWEBSHELLSERVICES \
  NS_IMETHOD ReloadDocument(const char *aCharset=nsnull, PRInt32 aSource=kCharsetUninitialized); \
  NS_IMETHOD StopDocumentLoad(void); \

#endif /* nsIWebShellServices_h___ */
