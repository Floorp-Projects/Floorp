/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package com.netscape.sasl;

/**
 * This class represents an error that has occurred when using SASL.
 *
 */
public class SaslException extends java.io.IOException {
    /**
     * The possibly null root cause exception.
     * @serial
     */
    private Throwable exception;

    /**
     * Constructs a new instance of <tt>SaslException</tt>.
     * The root exception and the detailed message are null.
     */
    public SaslException () {
        super();
    }

    /**
     * Constructs a new instance of <tt>SaslException</tt> with a detailed message.
     * The root exception is null.
     * @param detail A possibly null string containing details of the exception.
     *
     * @see java.lang.Throwable#getMessage
     */
    public SaslException (String detail) {
        super(detail);
    }

    /**
     * Constructs a new instance of <tt>SaslException</tt> with a detailed message
     * and a root exception.
     * For example, a SaslException might result from a problem with
     * the callback handler, which might throw a NoSuchCallbackException if
     * it does not support the requested callback, or throw an IOException
     * if it had problems obtaining data for the callback. The
     * SaslException's root exception would be then be the exception thrown
     * by the callback handler.
     *
     * @param detail A possibly null string containing details of the exception.
     * @param ex A possibly null root exception that caused this exception.
     *
     * @see java.lang.Throwable#getMessage
     * @see #getException
     */
    public SaslException (String detail, Throwable ex) {
        super(detail);
        exception = ex;
    }

    /**
     * Returns the root exception that caused this exception.
     * @return The possibly null root exception that caused this exception.
     */
    public Throwable getException() {
        return exception;
    }

    /**
     * Prints this exception's stack trace to <tt>System.err</tt>.
     * If this exception has a root exception; the stack trace of the
     * root exception is printed to <tt>System.err</tt> instead.
     */
    public void printStackTrace() {
        printStackTrace( System.err );
    }

    /**
     * Prints this exception's stack trace to a print stream.
     * If this exception has a root exception; the stack trace of the
     * root exception is printed to the print stream instead.
     * @param ps The non-null print stream to which to print.
     */
    public void printStackTrace(java.io.PrintStream ps) {
    if ( exception != null ) {
        String superString = super.toString();
        synchronized ( ps ) {
            ps.print(superString
                     + (superString.endsWith(".") ? "" : ".")
                     + "  Root exception is ");
            exception.printStackTrace( ps );
        }
    } else {
        super.printStackTrace( ps );
    }
    }

    /**
     * Prints this exception's stack trace to a print writer.
     * If this exception has a root exception; the stack trace of the
     * root exception is printed to the print writer instead.
     * @param ps The non-null print writer to which to print.
     */
    public void printStackTrace(java.io.PrintWriter pw) {
        if ( exception != null ) {
            String superString = super.toString();
            synchronized (pw) {
                pw.print(superString
                         + (superString.endsWith(".") ? "" : ".")
                         + "  Root exception is ");
                exception.printStackTrace( pw );
            }
        } else {
            super.printStackTrace( pw );
        }
    }

    /**
     * Returns the string representation of this exception.
     * The string representation contains
     * this exception's class name, its detailed messsage, and if
     * it has a root exception, the string representation of the root
     * exception. This string representation
     * is meant for debugging and not meant to be interpreted
     * programmatically.
     * @return The non-null string representation of this exception.
     * @see java.lang.Throwable#getMessage
     */
    public String toString() {
        String answer = super.toString();
        if (exception != null && exception != this) {
            answer += " [Root exception is " + exception.toString() + "]";
        }
        return answer;
    }
}
