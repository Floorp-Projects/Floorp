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
 * The Original Code is an API for accessing OS/2 Unicode support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#include "nsOS2Uni.h"

int WideCharToMultiByte( int CodePage, const PRUnichar *pText, ULONG ulLength, char* szBuffer, ULONG ulSize )
{
  UconvObject Converter = OS2Uni::GetUconvObject(CodePage);

  UniChar *ucsString = (UniChar*) pText;
  size_t   ucsLen = ulLength;
  size_t   cplen = ulSize;
  size_t   cSubs = 0;

  char *tmp = szBuffer; // function alters the out pointer

  int unirc = ::UniUconvFromUcs( Converter, &ucsString, &ucsLen,
                                 (void**) &tmp, &cplen, &cSubs);

  if( unirc != ULS_SUCCESS )
    return 0;

  if( unirc == UCONV_E2BIG)
  {
    // terminate output string (truncating)
    *(szBuffer + ulSize - 1) = '\0';
  }

  return ulSize - cplen;
}

int MultiByteToWideChar( int CodePage, const char*pText, ULONG ulLength, PRUnichar *szBuffer, ULONG ulSize )
{
  UconvObject Converter = OS2Uni::GetUconvObject(CodePage);

  char *ucsString = (char*) pText;
  size_t   ucsLen = ulLength;
  size_t   cplen = ulSize;
  size_t   cSubs = 0;

  PRUnichar *tmp = szBuffer; // function alters the out pointer

  int unirc = ::UniUconvToUcs( Converter, (void**)&ucsString, &ucsLen,
                               NS_REINTERPRET_CAST(UniChar**, &tmp),
                               &cplen, &cSubs);
                               
  if( unirc != ULS_SUCCESS )
    return 0;

  if( unirc == UCONV_E2BIG)
  {
    // terminate output string (truncating)
    *(szBuffer + ulSize - 1) = '\0';
  }

  return ulSize - cplen;
}

nsHashtable OS2Uni::gUconvObjects;

UconvObject
OS2Uni::GetUconvObject(int CodePage)
{
  nsPRUint32Key key(CodePage);
  UconvObject uco = OS2Uni::gUconvObjects.Get(&key);
  if (!uco) {
    UniChar codepage[20];
    int unirc = ::UniMapCpToUcsCp(CodePage, codepage, 20);
    if (unirc == ULS_SUCCESS) {
       unirc = ::UniCreateUconvObject(codepage, &uco);
       if (unirc == ULS_SUCCESS) {
          uconv_attribute_t attr;

          ::UniQueryUconvObject(uco, &attr, sizeof(uconv_attribute_t), 
                                NULL, NULL, NULL);
          attr.options = UCONV_OPTION_SUBSTITUTE_BOTH;
          attr.subchar_len=1;
          attr.subchar[0]='?';
          ::UniSetUconvObject(uco, &attr);
          OS2Uni::gUconvObjects.Put(&key, uco);
       }
    }
  }
  return uco;
}

PR_STATIC_CALLBACK(PRIntn)
UconvObjectEnum(nsHashKey* hashKey, void *aData, void* closure)
{
  UniFreeUconvObject((UconvObject)aData);
  return kHashEnumerateRemove;
}

void OS2Uni::FreeUconvObjects()
{
  if (gUconvObjects.Count()) {
    gUconvObjects.Enumerate(UconvObjectEnum, nsnull);
  }
}
