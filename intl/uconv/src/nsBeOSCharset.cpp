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
 *
 * Original code by:
 * Sergei Dolgov (sergei_d@fi.tartu.ee)
 * Tartu University
 * (C) 2002
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

#include "nsIPlatformCharset.h"
#include "nsReadableUtils.h"
#include "nsPlatformCharset.h"

// In BeOS, each window runs in its own thread.  Because of this,
// we have a proxy layer between the mozilla UI thread, and calls made
// within the window's thread via CallMethod().  However, since the windows
// are still running in their own thread, and reference counting takes place within
// that thread, we need to reference and de-reference outselves atomically.
// See BugZilla Bug# 92793
NS_IMPL_THREADSAFE_ISUPPORTS1(nsPlatformCharset, nsIPlatformCharset)

nsPlatformCharset::nsPlatformCharset()
{
  mCharset.AssignLiteral("UTF-8");
}

nsPlatformCharset::~nsPlatformCharset()
{
}

NS_IMETHODIMP
nsPlatformCharset::GetCharset(nsPlatformCharsetSel selector, nsACString& aResult)
{
  aResult = mCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsPlatformCharset::GetDefaultCharsetForLocale(const nsAString& localeName, nsACString& aResult)
{
  aResult = mCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsPlatformCharset::Init()
{
  return NS_OK;
}
nsresult
nsPlatformCharset::MapToCharset(short script, short region, nsACString& aCharset)
{
  return NS_OK;
}

nsresult
nsPlatformCharset::MapToCharset(nsAString& inANSICodePage, nsACString& aCharset)
{
  return NS_OK;
}

nsresult
nsPlatformCharset::InitGetCharset(nsACString &aString)
{
  aString = mCharset;
  return NS_OK;
}

nsresult
nsPlatformCharset::ConvertLocaleToCharsetUsingDeprecatedConfig(nsAString& locale, nsACString& aResult)
{
  aResult = mCharset;
  return NS_OK;
}

nsresult
nsPlatformCharset::VerifyCharset(nsCString &aCharset)
{
  aCharset = mCharset;
  return NS_OK;
}

nsresult
nsPlatformCharset::InitInfo()
{
  return NS_OK;
}
