/* -*- Mode: C; tab-width: 8 -*-
 * Copyright (C) 1998 Netscape Communications Corporation, All Rights Reserved.
 */
/* ** */

package netscape.javascript;
import java.io.*;

public class JSUtil {

    /* Return the stack trace of an exception or error as a String */
    public static String getStackTrace(Throwable t) {
	ByteArrayOutputStream captureStream;
	PrintStream p;
	
	captureStream = new ByteArrayOutputStream();
	p = new PrintStream(captureStream);

	t.printStackTrace(p);
	p.flush();

	return captureStream.toString();
    }
}
