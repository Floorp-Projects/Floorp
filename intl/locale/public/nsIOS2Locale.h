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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henry Sobotka <sobotka@axess.com> January 2000 update
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

#ifndef _nsios2locale_h_
#define _nsios2locale_h_

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include <os2.h>

// {F25F74F0-FB59-11d3-A9F2-00203522A03C}
#define NS_IOS2LOCALE_IID	                    \
{ 0xf25f74f0, 0xfb59, 0x11d3,                     \
{ 0xa9, 0xf2, 0x0, 0x20, 0x35, 0x22, 0xa0, 0x3c }}


class nsIOS2Locale : public nsISupports
{
 public:

   NS_DEFINE_STATIC_IID_ACCESSOR(NS_IOS2LOCALE_IID)
   NS_IMETHOD GetPlatformLocale(const nsAString& locale, PULONG os2Codepage) = 0;
   NS_IMETHOD GetXPLocale(const char* os2Locale, nsAString& locale)=0;
};

#endif
