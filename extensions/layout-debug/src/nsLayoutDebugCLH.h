/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
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
 * The Original Code is layout debugging code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
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

#ifndef nsLayoutDebugCLH_h_
#define nsLayoutDebugCLH_h_

#include "nsICommandLineHandler.h"
#define ICOMMANDLINEHANDLER nsICommandLineHandler

#define NS_LAYOUTDEBUGCLH_CID \
 { 0xa8f52633, 0x5ecf, 0x424a, \
   { 0xa1, 0x47, 0x47, 0xc3, 0x22, 0xf7, 0xbc, 0xe2 }}

class nsLayoutDebugCLH : public ICOMMANDLINEHANDLER
{
public:
    nsLayoutDebugCLH();
    virtual ~nsLayoutDebugCLH();

    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMMANDLINEHANDLER
};

#endif /* !defined(nsLayoutDebugCLH_h_) */
