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

#include "VariableBinding.h"


/**
 * Creates a variable binding with no name or value
**/
VariableBinding::VariableBinding() {
    this->allowShadow = MB_FALSE;
    this->value = 0;
    this->shadowValue = 0;
} //-- VariableBinding

/**
* Creates a variable binding with the given name, and not bound to any value
**/
VariableBinding::VariableBinding(const String& name) {
    this->name = name;
    this->allowShadow = MB_FALSE;
    this->value = 0;
    this->shadowValue = 0;
} //-- VariableBinding

/**
* Creates a variable binding with the given name, and value
**/
VariableBinding::VariableBinding(const String& name, ExprResult* value) {
    this->name.append(name);
    this->allowShadow = MB_FALSE;
    this->value = value;
    this->shadowValue = 0;
} //-- Variable

/**
* Destroys this VariableBinding
**/
VariableBinding::~VariableBinding() {
    delete shadowValue;
    delete value;
} //-- ~Variable

/**
* Allows this variable to be shadowed by another variable
**/
void VariableBinding::allowShadowing() {
    allowShadow = MB_TRUE;
} //-- allowShadowing

/**
* Disallows this variable to be shadowed by another variable
**/
void VariableBinding::disallowShadowing() {
    allowShadow = MB_FALSE;
} //-- disallowShadowing

/**
* Returns the name of this variable
**/
const String& VariableBinding::getName() {
    return (const String&)name;
} //-- getName

/**
* Returns the value of this variable
**/
ExprResult* VariableBinding::getValue() {
    if (shadowValue) return shadowValue;
    return value;
} //-- getValue

/**
* Returns the flag indicating whether variable shadowing is allowed
**/
MBool VariableBinding::isShadowingAllowed() {
    return allowShadow;
} //-- isShadowingAllowed

/**
* Sets the name of this variable
**/
void VariableBinding::setName(const String& name) {
    this->name = name;
} //-- setName

/**
 * Sets the shadow value of this binding if shadowing is turned on
**/
void VariableBinding::setShadowValue(ExprResult* value) {
    if (allowShadow) this->shadowValue = value;
} //-- setShadowValue

/**
* Sets the value of this variable
**/
void VariableBinding::setValue(ExprResult* value) {
    this->value = value;
} //-- setValue

