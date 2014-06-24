/******* BEGIN LICENSE BLOCK *******
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
 * The Initial Developers of the Original Code are Kevin Hendricks (MySpell)
 * and László Németh (Hunspell). Portions created by the Initial Developers
 * are Copyright (C) 2002-2005 the Initial Developers. All Rights Reserved.
 * 
 * Contributor(s): Benjamin Smedberg (benjamin@smedbergs.us) (Original Code)
 *                 László Németh (nemethl@gyorsposta.hu)
 *                 Ryan VanderMeulen (ryanvm@gmail.com)
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
 ******* END LICENSE BLOCK *******/

#ifndef mozHunspellDirProvider_h__
#define mozHunspellDirProvider_h__

#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/Attributes.h"

class mozHunspellDirProvider MOZ_FINAL :
  public nsIDirectoryServiceProvider2
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

  static char const *const kContractID;

private:
  ~mozHunspellDirProvider() {}

  class AppendingEnumerator MOZ_FINAL : public nsISimpleEnumerator
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    AppendingEnumerator(nsISimpleEnumerator* aBase);

  private:
    ~AppendingEnumerator() {}

    nsCOMPtr<nsISimpleEnumerator> mBase;
    nsCOMPtr<nsIFile>             mNext;
  };
};

#define HUNSPELLDIRPROVIDER_CID \
{ 0x64d6174c, 0x1496, 0x4ffd, \
  { 0x87, 0xf2, 0xda, 0x26, 0x70, 0xf8, 0x89, 0x34 } }

#endif // mozHunspellDirProvider
