/*
 * MessageOutputStream.java
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Knife.
 *
 * The Initial Developer of the Original Code is dog.
 * Portions created by dog are
 * Copyright (C) 1998 dog <dog@dog.net.uk>. All
 * Rights Reserved.
 *
 * Contributor(s): n/a.
 */

package dog.mail.util;

import java.io.*;

/**
 * An output stream that escapes any dots on a line by themself with
 * another dot, for the purposes of sending messages to SMTP and NNTP servers.
 *
 * @author dog@dog.net.uk
 * @version 0.1
 */
public class MessageOutputStream extends FilterOutputStream {

	/**
	 * The stream termination octet.
	 */
	public static final int END = 46;
	
	/**
	 * The line termination octet.
	 */
	public static final int LF = 10;

	int last = -1; // the last character written to the stream
	
	/**
	 * Constructs a message output stream connected to the specified output stream.
	 * @param out the target output stream
	 */
	public MessageOutputStream(OutputStream out) {
		super(out);
	}

	/**
	 * Writes a character to the underlying stream.
	 * @exception IOException if an I/O error occurred
	 */
	public void write(int ch) throws IOException {
		if ((last==LF || last==-1) && ch==END)
			out.write(END); // double any lonesome dots
		super.write(ch);
	}

	/**
	 * Writes a portion of a byte array to the underlying stream.
	 * @exception IOException if an I/O error occurred
	 */
	public void write(byte b[], int off, int len) throws IOException {
		int k = last != -1 ? last : LF;
		int l = off;
		len += off;
		for (int i = off; i < len; i++) {
			if (k==LF && b[i]==END) {
				super.write(b, l, i-l);
				out.write(END);
				l = i;
			}
			k = b[i];
		}
		if (len-l > 0)
			super.write(b, l, len-l);
	}
	
}
