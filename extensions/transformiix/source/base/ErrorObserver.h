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

#ifndef MITRE_ERROROBSERVER_H
#define MITRE_ERROROBSERVER_H

#include "baseutils.h"
#include "String.h"
#include "iostream.h"

/**
 * A simple interface for observing errors
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
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
