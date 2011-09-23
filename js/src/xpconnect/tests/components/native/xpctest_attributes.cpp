/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
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

#include "xpctest_private.h"

NS_IMPL_ISUPPORTS1(xpcTestObjectReadOnly, nsIXPCTestObjectReadOnly)

xpcTestObjectReadOnly :: xpcTestObjectReadOnly() {
    boolProperty = PR_TRUE;
    shortProperty = 32767;
    longProperty =  2147483647;
    floatProperty = 5.5f;
    charProperty = 'X';
}

NS_IMETHODIMP xpcTestObjectReadOnly :: GetStrReadOnly(char * *aStrReadOnly){
    char aString[] = "XPConnect Read-Only String";

    if(!aStrReadOnly)
        return NS_ERROR_NULL_POINTER;
    *aStrReadOnly = (char*) nsMemory::Clone(aString,
                                            sizeof(char)*(strlen(aString)+1));
    return *aStrReadOnly ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP xpcTestObjectReadOnly :: GetBoolReadOnly(PRBool *aBoolReadOnly) {
    *aBoolReadOnly = boolProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadOnly :: GetShortReadOnly(PRInt16 *aShortReadOnly){
    *aShortReadOnly = shortProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadOnly :: GetLongReadOnly(PRInt32 *aLongReadOnly){
    *aLongReadOnly = longProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadOnly :: GetFloatReadOnly(float *aFloatReadOnly){
    *aFloatReadOnly = floatProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadOnly :: GetCharReadOnly(char *aCharReadOnly){
    *aCharReadOnly = charProperty;
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(xpcTestObjectReadWrite, nsIXPCTestObjectReadWrite)

xpcTestObjectReadWrite :: xpcTestObjectReadWrite() {
    const char s[] = "XPConnect Read-Writable String";
    stringProperty = (char*) nsMemory::Clone(s, sizeof(char)*(strlen(s)+1));
    boolProperty = PR_TRUE;
    shortProperty = 32767;
    longProperty =  2147483647;
    floatProperty = 5.5f;
    charProperty = 'X';
}

xpcTestObjectReadWrite :: ~xpcTestObjectReadWrite()
{
    nsMemory::Free(stringProperty);
}

NS_IMETHODIMP xpcTestObjectReadWrite :: GetStringProperty(char * *aStringProperty) {
    if(!aStringProperty)
        return NS_ERROR_NULL_POINTER;
    *aStringProperty = (char*) nsMemory::Clone(stringProperty, 
                                               sizeof(char)*(strlen(stringProperty)+1));
    return *aStringProperty ? NS_OK : NS_ERROR_OUT_OF_MEMORY;

}
NS_IMETHODIMP xpcTestObjectReadWrite :: SetStringProperty(const char * aStringProperty) {
    nsMemory::Free(stringProperty);
    stringProperty = (char*) nsMemory::Clone(aStringProperty,
                                             sizeof(char)*(strlen(aStringProperty)+1));
    return NS_OK;
}

NS_IMETHODIMP xpcTestObjectReadWrite :: GetBooleanProperty(PRBool *aBooleanProperty) {
    *aBooleanProperty = boolProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: SetBooleanProperty(PRBool aBooleanProperty) {
    NS_ENSURE_TRUE(aBooleanProperty == PR_TRUE || aBooleanProperty == PR_FALSE,
                   NS_ERROR_INVALID_ARG);
    boolProperty = aBooleanProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: GetShortProperty(PRInt16 *aShortProperty) {
    *aShortProperty = shortProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: SetShortProperty(PRInt16 aShortProperty) {
    shortProperty = aShortProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: GetLongProperty(PRInt32 *aLongProperty) {
    *aLongProperty = longProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: SetLongProperty(PRInt32 aLongProperty) {
    longProperty = aLongProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: GetFloatProperty(float *aFloatProperty) {
    *aFloatProperty = floatProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: SetFloatProperty(float aFloatProperty) {
    floatProperty = aFloatProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: GetCharProperty(char *aCharProperty) {
    *aCharProperty = charProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: SetCharProperty(char aCharProperty) {
    charProperty = aCharProperty;
    return NS_OK;
}
