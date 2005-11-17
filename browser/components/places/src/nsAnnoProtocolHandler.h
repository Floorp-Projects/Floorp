//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Annotation Service
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
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

#ifndef nsAnnoProtocolHandler_h___
#define nsAnnoProtocolHandler_h___

#include "nsCOMPtr.h"
#include "nsIAnnotationService.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsWeakReference.h"

// {e8b8bdb7-c96c-4d82-9c6f-2b3c585ec7ea}
#define NS_ANNOPROTOCOLHANDLER_CID \
{ 0xe8b8bdb7, 0xc96c, 0x4d82, { 0x9c, 0x6f, 0x2b, 0x3c, 0x58, 0x5e, 0xc7, 0xea } }

class nsAnnoProtocolHandler : public nsIProtocolHandler, public nsSupportsWeakReference
{
public:
  nsAnnoProtocolHandler() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

private:
  ~nsAnnoProtocolHandler() {}

protected:
  nsresult ParseAnnoURI(nsIURI* aURI, nsIURI** aResultURI, nsCString& aName);
};

#endif /* nsAnnoProtocolHandler_h___ */
