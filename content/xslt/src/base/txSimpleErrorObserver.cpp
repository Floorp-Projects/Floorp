/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
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
