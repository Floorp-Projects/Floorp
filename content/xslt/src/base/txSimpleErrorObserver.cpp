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
 * $Id: txSimpleErrorObserver.cpp,v 1.2 1999/11/15 07:12:41 nisheeth%netscape.com Exp $
 */

#include "ErrorObserver.h"

/**
 * Creates a new SimpleErrorObserver.
 * All errors will be printed to the console (cout).
**/
SimpleErrorObserver::SimpleErrorObserver() {
    errStream = &cout;
    hideWarnings = MB_FALSE;
} //-- SimpleErrorObserver

/**
 * Creates a new SimpleErrorObserver.
 * All errors will be printed to the given ostream.
**/
SimpleErrorObserver::SimpleErrorObserver(ostream& errStream) {
    this->errStream = &errStream;
    hideWarnings = MB_FALSE;
} //-- SimpleErrorObserver

/**
 *  Notifies this Error observer of a new error, with default
 *  level of NORMAL
**/
void SimpleErrorObserver::recieveError(String& errorMessage) {
    *errStream << "error: " << errorMessage << endl;
    errStream->flush();
} //-- recieveError

/**
 *  Notifies this Error observer of a new error using the given error level
**/
void SimpleErrorObserver::recieveError(String& errorMessage, ErrorLevel level) {


    switch ( level ) {
        case ErrorObserver::FATAL :
            *errStream << "fatal error: ";
            break;
        case ErrorObserver::WARNING :
            if ( hideWarnings ) return;
            *errStream << "warning: ";
            break;
        default:
            *errStream << "error: ";
            break;
    }

    *errStream << errorMessage << endl;
    errStream->flush();
} //-- recieveError

void SimpleErrorObserver::supressWarnings(MBool supress) {
    this->hideWarnings = supress;
} //-- supressWarnings

