/* -*- Mode: java; tab-width: 8 -*-
 * Copyright © 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

/**
 * A wrapper for runtime exceptions.
 *
 * Used by the JavaScript runtime to wrap and propagate exceptions that occur
 * during runtime.
 *
 * @author Norris Boyd
 */
public class WrappedException extends RuntimeException {

    /**
     * Create a new exception wrapped around an existing exception.
     *
     * @param exception the exception to wrap
     */
    public WrappedException(Throwable exception) {
        super(exception.getMessage());
        this.exception = exception.fillInStackTrace();
    }

    /**
     * Get the message for the exception.
     *
     * Delegates to the wrapped exception.
     */
    public String getMessage() {
        return "WrappedException of " + exception.toString();
    }

    /**
     * Get the wrapped exception.
     *
     * @return the exception that was presented as a argument to the
     *         constructor when this object was created
     */
    public Throwable getWrappedException() {
        return exception;
    }

    /**
     * Get the wrapped exception.
     *
     * @return the exception that was presented as a argument to the
     *         constructor when this object was created
     */
    public Object unwrap() {
        return exception;
    }

    private Throwable exception;
}
