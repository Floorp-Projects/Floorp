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
 * $Id: txErrorObserver.h,v 1.4 2005/11/02 07:33:52 peterv%netscape.com Exp $
 */

#ifndef MITRE_ERROROBSERVER_H
#define MITRE_ERROROBSERVER_H

#include "baseutils.h"
#include "TxString.h"
#include <iostream.h>

/**
 * A simple interface for observing errors
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.4 $ $Date: 2005/11/02 07:33:52 $
**/
class ErrorObserver {

public:

    enum ErrorLevel {FATAL = 0, NORMAL, WARNING};

    /**
     * Default Destructor for ErrorObserver
    **/
    virtual ~ErrorObserver() {};

    /**
     *  Notifies this Error observer of a new error, with default
     *  level of NORMAL
    **/
    virtual void recieveError(String& errorMessage) = 0;

    /**
     *  Notifies this Error observer of a new error using the given error level
    **/
    virtual void recieveError(String& errorMessage, ErrorLevel level) = 0;

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
     *  Notifies this Error observer of a new error, with default
     *  level of NORMAL
    **/
    virtual void recieveError(String& errorMessage);

    /**
     *  Notifies this Error observer of a new error using the given error level
    **/
    virtual void recieveError(String& errorMessage, ErrorLevel level);

    virtual void supressWarnings(MBool supress);

private:

    ostream* errStream;
    MBool hideWarnings;
}; //-- SimpleErrorObserver
#endif

