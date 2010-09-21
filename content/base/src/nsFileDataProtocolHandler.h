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
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsFileDataProtocolHandler_h___
#define nsFileDataProtocolHandler_h___

#include "nsIProtocolHandler.h"

#define FILEDATA_SCHEME "moz-filedata"

class nsIDOMFile;
class nsIPrincipal;

class nsFileDataProtocolHandler : public nsIProtocolHandler
{
public:
  NS_DECL_ISUPPORTS

  // nsIProtocolHandler methods:
  NS_DECL_NSIPROTOCOLHANDLER

  // nsFileDataProtocolHandler methods:
  nsFileDataProtocolHandler() {}
  virtual ~nsFileDataProtocolHandler() {}

  // Methods for managing uri->file mapping
  static void AddFileDataEntry(nsACString& aUri,
			       nsIDOMFile* aFile,
                               nsIPrincipal* aPrincipal);
  static void RemoveFileDataEntry(nsACString& aUri);
  
};

#define NS_FILEDATAPROTOCOLHANDLER_CID \
{ 0xb43964aa, 0xa078, 0x44b2, \
  { 0xb0, 0x6b, 0xfd, 0x4d, 0x1b, 0x17, 0x2e, 0x66 } }

#endif /* nsFileDataProtocolHandler_h___ */
