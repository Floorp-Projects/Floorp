/*
 * MessageInputStream.java
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
 * A utility class for feeding message contents to messages.
 *
 * @author dog@dog.net.uk
 * @version 0.1
 */
public class MessageInputStream extends FilterInputStream {

	/**
	 * The stream termination octet.
	 */
	public static final int END = 46;
	
	/**
	 * The line termination octet.
	 */
	public static final int LF = 10;
	
	boolean done = false;
	int last = -1; // register for the last octet read
	
	/**
	 * Constructs a message input stream connected to the specified pushback input stream.
	 */
	public MessageInputStream(PushbackInputStream in) {
		super(in);
	}

	/**
	 * Reads the next byte of data from this message input stream.
	 * Returns -1 if the end of the message stream has been reached.
	 * @exception IOException if an I/O error occurs
	 */
	public int read() throws IOException {
		if (done) return -1;
		int ch = in.read();
		if (ch==END && last==LF) {
			int ch2 = in.read(); // look ahead for LF
			if (ch2==LF) {
				done = true;
				return -1; // swallow the END and LF
			} else
				((PushbackInputStream)in).unread(ch2);
		}
		last = ch;
		return ch;
	}
	
	/**
	 * Reads up to b.length bytes of data from this input stream into
	 * an array of bytes.
	 * Returns -1 if the end of the stream has been reached.
	 * @exception IOException if an I/O error occurs
	 */
	public int read(byte[] b) throws IOException {
		return read(b, 0, b.length);
	}
	
	/**
	 * Reads up to len bytes of data from this input stream into an
	 * array of bytes, starting at the specified offset.
	 * Returns -1 if the end of the stream has been reached.
	 * @exception IOException if an I/O error occurs
	 */
	public int read(byte[] b, int off, int len) throws IOException {
        if (done) return -1;
		int l = in.read(b, off, len);
		int i = indexOfEnd(b, off, l);
		if (i>-1) {
			((PushbackInputStream)in).unread(b, i+2, (l-(off+i+2)));
			done = true;
			return i-off;
		}
		return l;
	}

    // Discover the index of END in a byte array.
	int indexOfEnd(byte[] b, int off, int len) {
		for (int i=off+2; i<(off+len); i++)
			if (b[i]==LF && b[i-1]==END && b[i-2]==LF)
				return i-1;
		return -1;
	}

}