/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999, 2000 Keith Visco.
 * All Rights Reserved..
 *
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */


#ifndef TRANSFRMX_TXOBJECT_H
#define TRANSFRMX_TXOBJECT_H

#include "baseutils.h"

class TxObject {

 public:

    /**
     * Creates a new TxObject
    **/
    TxObject() {};

    /**
     * Deletes this TxObject
    **/
    virtual ~TxObject() {};

    /**
     * Returns the Hashcode for this TxObject
    **/
    virtual PRInt32 hashCode() {
        return NS_PTR_TO_INT32(this);
    } //-- hashCode

    /**
     * Returns true if the given Object is equal to this object.
     * By default the comparision operator == is used, but this may
     * be overridden
    **/
    virtual MBool equals(TxObject* obj) {
        return (MBool)(obj == this);
    } //-- equals
};

/**
 * A Simple TxObject wrapper class
**/
class TxObjectWrapper : public TxObject {
public:
    TxObjectWrapper();
    virtual ~TxObjectWrapper();
    void* object;
};

#endif
