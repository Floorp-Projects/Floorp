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

#ifndef MITRE_ERROROBSERVER_H
#define MITRE_ERROROBSERVER_H

#include "baseutils.h"
#include "txError.h"
#include <iostream.h>

class String;

/**
 * A simple interface for observing errors
**/
class ErrorObserver {

public:

    /**
     * Default Destructor for ErrorObserver
    **/
    virtual ~ErrorObserver() {};

    /**
     *  Notifies this Error observer of a new error aRes
    **/
    virtual void receiveError(const String& errorMessage, nsresult aRes) = 0;

    /**
     *  Notifies this Error observer of a new error, with default
     *  error code NS_ERROR_FAILURE
    **/
    void receiveError(String& errorMessage)
    {
        receiveError(errorMessage, NS_ERROR_FAILURE);
    }

        

}; //-- ErrorObserver

/**
 * A simple ErrorObserver which allows printing error messages to a stream
**/
class SimpleErrorObserver : public ErrorObserver {

public:

    /**
     * Creates a new SimpleErrorObserver.
     * All errors will be printed to the console (cout).
    **/
    SimpleErrorObserver();

    /**
     * Creates a new SimpleErrorObserver.
     * All errors will be printed to the given ostream.
    **/
    SimpleErrorObserver(ostream& errStream);

    virtual ~SimpleErrorObserver() {};

    /**
     *  Notifies this Error observer of a new error aRes
    **/
    void receiveError(const String& errorMessage, nsresult aRes);

    virtual void supressWarnings(MBool supress);

private:

    ostream* errStream;
    MBool hideWarnings;
}; //-- SimpleErrorObserver
#endif

