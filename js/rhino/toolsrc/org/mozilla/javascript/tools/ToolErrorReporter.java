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

package org.mozilla.javascript.tools;

import org.mozilla.javascript.*;
import java.text.MessageFormat;
import java.io.*;
import java.util.*;

/**
 * Error reporter for tools.
 *
 * Currently used by both the shell and the compiler.
 */
public class ToolErrorReporter implements ErrorReporter {

    public ToolErrorReporter(boolean reportWarnings) {
	this.reportWarnings = reportWarnings;
    }

    /**
     * Look up the message corresponding to messageId in the
     * org.mozilla.javascript.tools.shell.resources.Messages property file.
     * For internationalization support.
     */
    public static String getMessage(String messageId) {
        return getMessage(messageId, (Object []) null);
    }

    public static String getMessage(String messageId, String argument) {
        Object[] args = { argument };
        return getMessage(messageId, args);
    }

    public static String getMessage(String messageId, Object[] args) {
        Context cx = Context.getCurrentContext();
        Locale locale = cx == null ? Locale.getDefault() : cx.getLocale();

        // ResourceBundle does cacheing.
        ResourceBundle rb = ResourceBundle.getBundle
            ("org.mozilla.javascript.tools.resources.Messages", locale);

        String formatString;
        try {
            formatString = rb.getString(messageId);
        } catch (java.util.MissingResourceException mre) {
            throw new RuntimeException("no message resource found for message property "
                                       + messageId);
        }

        if (args == null) {
            return formatString;
        } else {
            MessageFormat formatter = new MessageFormat(formatString);
            return formatter.format(args);
        }
    }

    public void warning(String message, String sourceName, int line,
                        String lineSource, int lineOffset)
    {
        if (!reportWarnings)
            return;
        Object[] errArgs = { formatMessage(message, sourceName, line) };
        message = getMessage("msg.warning", errArgs);
        System.err.println(messagePrefix + message);
        if (null != lineSource) {
            System.err.println(messagePrefix + lineSource);
            System.err.println(messagePrefix + buildIndicator(lineOffset));
        }
    }

    public void error(String message, String sourceName, int line,
                      String lineSource, int lineOffset)
    {
        hasReportedErrorFlag = true;
        message = formatMessage(message, sourceName, line);
        System.err.println(messagePrefix + message);
        if (null != lineSource) {
            System.err.println(messagePrefix + lineSource);
            System.err.println(messagePrefix + buildIndicator(lineOffset));
        }
    }

    public EvaluatorException runtimeError(String message, String sourceName,
                                           int line, String lineSource,
                                           int lineOffset)
    {
        hasReportedErrorFlag = true;
        message = formatMessage(message, sourceName, line);
        System.err.println(messagePrefix + message);
        if (null != lineSource) {
            System.err.println(messagePrefix + lineSource);
            System.err.println(messagePrefix + buildIndicator(lineOffset));
        }
        return new EvaluatorException(message);
    }

    public boolean hasReportedError() {
        return hasReportedErrorFlag;
    }

    public boolean isReportingWarnings() {
        return this.reportWarnings;
    }

    public void setIsReportingWarnings(boolean reportWarnings) {
        this.reportWarnings = reportWarnings;
    }

    private String formatMessage(String message, String sourceName, int line) {
        if (line > 0) {
            if (sourceName != null) {
                Object[] errArgs = { sourceName, new Integer(line), message };
                return getMessage("msg.format3", errArgs);
            } else {
                Object[] errArgs = { new Integer(line), message };
                return getMessage("msg.format2", errArgs);
            }
        } else {
            Object[] errArgs = { message };
            return getMessage("msg.format1", errArgs);
        }
    }

    private String buildIndicator(int offset){
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < offset-1; i++)
            sb.append(".");
        sb.append("^");
        return sb.toString();
    }

    private final String messagePrefix = "js: ";
    private boolean hasReportedErrorFlag;
    private boolean reportWarnings; 
}
