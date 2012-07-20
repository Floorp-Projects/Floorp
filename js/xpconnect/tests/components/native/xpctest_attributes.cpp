/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpctest_private.h"

NS_IMPL_ISUPPORTS1(xpcTestObjectReadOnly, nsIXPCTestObjectReadOnly)

xpcTestObjectReadOnly :: xpcTestObjectReadOnly() {
    boolProperty = true;
    shortProperty = 32767;
    longProperty =  2147483647;
    floatProperty = 5.5f;
    charProperty = 'X';
    // timeProperty is PRTime and signed type.
    // So it has to allow negative value.
    timeProperty = -1;
}

NS_IMETHODIMP xpcTestObjectReadOnly :: GetStrReadOnly(char * *aStrReadOnly){
    char aString[] = "XPConnect Read-Only String";

    if (!aStrReadOnly)
        return NS_ERROR_NULL_POINTER;
    *aStrReadOnly = (char*) nsMemory::Clone(aString,
                                            sizeof(char)*(strlen(aString)+1));
    return *aStrReadOnly ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP xpcTestObjectReadOnly :: GetBoolReadOnly(bool *aBoolReadOnly) {
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
NS_IMETHODIMP xpcTestObjectReadOnly :: GetTimeReadOnly(PRTime *aTimeReadOnly){
    *aTimeReadOnly = timeProperty;
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(xpcTestObjectReadWrite, nsIXPCTestObjectReadWrite)

xpcTestObjectReadWrite :: xpcTestObjectReadWrite() {
    const char s[] = "XPConnect Read-Writable String";
    stringProperty = (char*) nsMemory::Clone(s, sizeof(char)*(strlen(s)+1));
    boolProperty = true;
    shortProperty = 32767;
    longProperty =  2147483647;
    floatProperty = 5.5f;
    charProperty = 'X';
    // timeProperty is PRTime and signed type.
    // So it has to allow negative value.
    timeProperty = -1;
}

xpcTestObjectReadWrite :: ~xpcTestObjectReadWrite()
{
    nsMemory::Free(stringProperty);
}

NS_IMETHODIMP xpcTestObjectReadWrite :: GetStringProperty(char * *aStringProperty) {
    if (!aStringProperty)
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

NS_IMETHODIMP xpcTestObjectReadWrite :: GetBooleanProperty(bool *aBooleanProperty) {
    *aBooleanProperty = boolProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: SetBooleanProperty(bool aBooleanProperty) {
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
NS_IMETHODIMP xpcTestObjectReadWrite :: GetTimeProperty(PRTime *aTimeProperty) {
    *aTimeProperty = timeProperty;
    return NS_OK;
}
NS_IMETHODIMP xpcTestObjectReadWrite :: SetTimeProperty(PRTime aTimeProperty) {
    timeProperty = aTimeProperty;
    return NS_OK;
}
