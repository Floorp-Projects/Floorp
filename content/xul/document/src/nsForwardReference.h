/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsForwardReference_h__
#define nsForwardReference_h__

class nsForwardReference
{
protected:
    nsForwardReference() {}

public:
    virtual ~nsForwardReference() {}

    enum Result {
        eResolveSucceeded, // resolution succeeded, i'm done
        eResolveLater,     // couldn't resolve, try me later
        eResolveError      // something bad happened, don't try again
    };

    /**
     * Attempt to resolve the forward reference.
     *
     * @return a Result that tells the resolver how to treat
     * the reference.
     */
    virtual Result Resolve() = 0;
};

#endif // nsForwardReference_h__
