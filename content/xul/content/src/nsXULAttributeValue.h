/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Edward Kandrot <kandrot@netscape.com>
 */

#ifndef nsXULAttributeValue_h__
#define nsXULAttributeValue_h__

#include "nsString.h"
#include "nsIAtom.h"

class nsXULAttributeValue
{
    void  *mValue;

    static const PRUint32 kMaxAtomValueLength;

    enum {
        kTypeMask   = 0x1,
        kStringType = 0x0,
        kAtomType   = 0x1
    };

    PRBool IsStringValue() {
        return (PRWord(mValue) & kTypeMask) == kStringType;
    }

public:
                nsXULAttributeValue();
                ~nsXULAttributeValue();

    void        ReleaseValue( void );

    nsresult    GetValue(nsAWritableString& aResult);

    nsresult    SetValue(const nsAReadableString& aValue, PRBool forceAtom=PR_FALSE);

    nsresult    GetValueAsAtom(nsIAtom** aResult);
};

#endif // nsXULAttributeValue_h__
