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
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

#ifndef MITREXSL_VARIABLE_H
#define MITREXSL_VARIABLE_H

#include "TxObject.h"
#include "TxString.h"
#include "ExprResult.h"

class VariableBinding : public TxObject {

public:

    /**
     * Creates a variable binding with no name or value
    **/
    VariableBinding();

    /**
     * Creates a variable binding with the given name, and not bound to any value
    **/
    VariableBinding(const String& name);

    /**
     * Creates a variable with the given name, and value
    **/
    VariableBinding(const String& name, ExprResult* value);


    /**
     * Destroys this variable binding
    **/
    virtual ~VariableBinding();

    /**
     * Allows this variable to be shadowed by another variable
    **/
    void allowShadowing();

    /**
     * Disallows this variable to be shadowed by another variable
    **/
    void disallowShadowing();

    /**
     * Returns the name of this variable
    **/
    const String& getName();

    /**
     * Returns the value of this variable, if shadowing is turned on
     * and the shadow value is not null, it will be returned
    **/
    ExprResult* getValue();

    /**
     * Returns the flag indicating whether variable shadowing is allowed
    **/
    MBool isShadowingAllowed();

    /**
     * Sets the name of this variable
    **/
    void setName(const String& name);

    /**
     * Sets the shadow value of this binding
    **/
    void setShadowValue(ExprResult* value);

    /**
     * Sets the value of this variable
    **/
    void setValue(ExprResult* value);



private:

    /**
     * flag to turn on and off shadowing
    **/
    MBool       allowShadow;

    /**
     * The name of this variable
    **/
    String      name;

    /**
     * The value of this variable
    **/
    ExprResult* value;

    ExprResult* shadowValue;
};

#endif
