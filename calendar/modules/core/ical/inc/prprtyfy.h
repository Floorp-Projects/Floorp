/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* 
 * prprtyfy.h
 * John Sun
 * 2/12/98 5:17:48 PM
 */

#include "prprty.h"

#ifndef __ICALPROPERTYFACTORY_H_
#define __ICALPROPERTYFACTORY_H_

/**
 * ICalPropertyFactory is a factory class that creates 
 * ICalProperty instances.
 *
 * @see             ICalProperty 
 */
class ICalPropertyFactory
{
private:

    /**
     * Hide default constructor from use.
     */
    ICalPropertyFactory(); 

public:

    /**
     * Factory method that creates ICalProperty objects.
     * Clients should call this method to create ICalProperty 
     * objects.  Clients are responsible for de-allocating returned
     * object.
     *
     * @param           aType       the type of ICALProperty
     * @param           value       the initial value of the property   
     * @param           parameters  the initial parameters of the property
     *
     * @return          ptr to the  newly created ICalProperty object
     * @see             ICalProperty 
    */
    static ICalProperty * Make(ICalProperty::PropertyTypes aType,
        void * value, JulianPtrArray * parameters);
};

#endif /* __ICALPROPERTYFACTORY_H_ */



