/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MITRE_ERROROBSERVER_H
#define MITRE_ERROROBSERVER_H

#include "txCore.h"

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
    virtual void receiveError(const nsAString& errorMessage, nsresult aRes) = 0;

    /**
     *  Notifies this Error observer of a new error, with default
     *  error code NS_ERROR_FAILURE
    **/
    void receiveError(const nsAString& errorMessage)
    {
        receiveError(errorMessage, NS_ERROR_FAILURE);
    }

        

}; //-- ErrorObserver

#endif
