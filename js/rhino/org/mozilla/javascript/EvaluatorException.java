/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


package org.mozilla.javascript;

/**
 * The class of exceptions thrown by the JavaScript engine.
 */
public class EvaluatorException extends RuntimeException {

    /**
     * Create an exception with the specified detail message.
     *
     * Errors internal to the JavaScript engine will simply throw a
     * RuntimeException.
     *
     * @param detail a message with detail about the exception
     */
    public EvaluatorException(String detail) {
        super(detail);
    }
}
