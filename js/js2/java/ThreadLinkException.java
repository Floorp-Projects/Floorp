/* -*- Mode: java; tab-width: 8 -*-
 * Copyright © 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

// API class
/**
 * Thrown if the thread association cannot be made.
 *
 * Thrown by Context.enter() if the context
 * is already associated with a thread, or if the
 * current thread is already associated with a context.<p>
 *
 * Thrown by Context.exit() if the context is not
 * associated with the current thread.
 *
 * @see com.netscape.javascript.Context#enter
 * @see com.netscape.javascript.Context#exit
 */
public class ThreadLinkException extends Exception {

    public ThreadLinkException(String detail) {
        super(detail);
    }
}

