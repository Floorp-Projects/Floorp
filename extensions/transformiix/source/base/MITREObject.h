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
 * $Id: MITREObject.h,v 1.3 2000/03/02 09:22:28 kvisco%ziplink.net Exp $
 */

#include "TxObject.h"

#ifndef TRANSFRMX_MITREOBJECT_H
#define TRANSFRMX_MITREOBJECT_H

/**
 * Note this class is here for backward compatiblity, since a number
 * of other classes rely on it. I primarily use TxObject now, which
 * contains the #hashCode method.
 * A standard base class for many of the Class definitions in this
 * application.
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.3 $ $Date: 2000/03/02 09:22:28 $
**/
class MITREObject : public TxObject{
public:
    MITREObject() {};
    virtual ~MITREObject() {};
};

/**
 * A Simple MITREObject wrapper class
**/
class MITREObjectWrapper : public MITREObject {
public:
    MITREObjectWrapper();
    virtual ~MITREObjectWrapper();
    void* object;
};

#endif

