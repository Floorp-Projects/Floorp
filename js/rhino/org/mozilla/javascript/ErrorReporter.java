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

// API class

package org.mozilla.javascript;

/**
 * This is interface defines a protocol for the reporting of
 * errors during JavaScript translation or execution.
 *
 * @author Norris Boyd
 */

public interface ErrorReporter {

    /**
     * Report a warning.
     *
     * The implementing class may choose to ignore the warning
     * if it desires.
     *
     * @param message a String describing the warning
     * @param sourceName a String describing the JavaScript source
     * where the warning occured; typically a filename or URL
     * @param line the line number associated with the warning
     * @param lineSource the text of the line (may be null)
     * @param lineOffset the offset into lineSource where problem was detected
     */
    void warning(String message, String sourceName, int line,
                 String lineSource, int lineOffset);

    /**
     * Report an error.
     *
     * The implementing class is free to throw an exception if
     * it desires.
     *
     * If execution has not yet begun, the JavaScript engine is
     * free to find additional errors rather than terminating
     * the translation. It will not execute a script that had
     * errors, however.
     *
     * @param message a String describing the error
     * @param sourceName a String describing the JavaScript source
     * where the error occured; typically a filename or URL
     * @param line the line number associated with the error
     * @param lineSource the text of the line (may be null)
     * @param lineOffset the offset into lineSource where problem was detected
     */
    void error(String message, String sourceName, int line,
               String lineSource, int lineOffset);

    /**
     * Creates an EvaluatorException that may be thrown.
     *
     * runtimeErrors, unlike errors, will always terminate the
     * current script.
     *
     * @param message a String describing the error
     * @param sourceName a String describing the JavaScript source
     * where the error occured; typically a filename or URL
     * @param line the line number associated with the error
     * @param lineSource the text of the line (may be null)
     * @param lineOffset the offset into lineSource where problem was detected
     * @return an EvaluatorException that will be thrown.
     */
    EvaluatorException runtimeError(String message, String sourceName, 
                                    int line, String lineSource, 
                                    int lineOffset);
}
