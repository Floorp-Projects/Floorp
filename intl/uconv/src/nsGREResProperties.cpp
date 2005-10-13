/* vim: set sts=2 sw=2 et cin: */
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
 * The Original Code is gre/res property file loading code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "nsGREResProperties.h"
#include "nsILocalFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetUtil.h"

nsGREResProperties::nsGREResProperties(const nsACString& aFile)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(file));
  if (NS_FAILED(rv))
    return;

  file->AppendNative(NS_LITERAL_CSTRING("res"));
  file->AppendNative(aFile);

  nsCOMPtr<nsILocalFile> lf(do_QueryInterface(file));
  NS_ENSURE_TRUE(lf, /**/);

  nsCOMPtr<nsIInputStream> inStr;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), lf);
  if (NS_FAILED(rv))
    return;

  mProps = do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);
  if (mProps) {
    rv = mProps->Load(inStr);
    if (NS_FAILED(rv))
      mProps = nsnull;
  }
}

PRBool nsGREResProperties::DidLoad() const
{
  return mProps != nsnull;
}

nsresult nsGREResProperties::Get(const nsAString& aKey, nsAString& aValue)
{
  if (!mProps)
    return NS_ERROR_NOT_INITIALIZED;

  return mProps->GetStringProperty(NS_ConvertUTF16toUTF8(aKey), aValue);
}
