/*
 * CRLFOutputStream.java
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
 * An output stream that filters LFs into CR/LF pairs.
 *
 * @author dog@dog.net.uk
 * @version 0.1
 */
public class CRLFOutputStream extends FilterOutputStream {

	/**
	 * The CR octet.
	 */
	public static final int CR = 13;
	
	/**
	 * The LF octet.
	 */
	public static final int LF = 10;
	
	/**
	 * The CR/LF pair.
	 */
	public static final byte[] CRLF = { CR, LF };
	
	/**
	 * The last byte read.
	 */
	protected int last;

	/**
	 * Constructs a CR/LF output stream connected to the specified output stream.
	 */
	public CRLFOutputStream(OutputStream out) {
		super(out);
		last = -1;
	}

	/**
	 * Writes a character to the underlying stream.
	 * @exception IOException if an I/O error occurred
	 */
	public void write(int ch) throws IOException {
		if (ch==CR)
			out.write(CRLF);
		else
			if (ch==LF) {
				if (last!=CR)
					out.write(CRLF);
			} else {
				out.write(ch);
			}
		last = ch;
	}

	/**
	 * Writes a byte array to the underlying stream.
	 * @exception IOException if an I/O error occurred
	 */
	public void write(byte b[]) throws IOException {
		write(b, 0, b.length);
	}

	/**
	 * Writes a portion of a byte array to the underlying stream.
	 * @exception IOException if an I/O error occurred
	 */
	public void write(byte b[], int off, int len) throws IOException {
		int d = off;
		len += off;
		for (int i=off; i<len; i++)
			switch (b[i]) {
			  default:
				break;
			  case CR:
				if (i+1<len && b[i+1]==LF) {
					i++;
				} else {
					out.write(b, d, (i-d)+1);
					out.write(LF);
					d = i+1;
				}
				break;
			  case LF:
				out.write(b, d, i-d);
				out.write(CRLF, 0, 2);
				d = i+1;
				break;
			}
		if (len-d>0)
			out.write(b, d, len-d);
	}

	/**
	 * Writes a newline to the underlying stream.
	 * @exception IOException if an I/O error occurred
	 */
	public void writeln() throws IOException {
		out.write(CRLF);
	}

}
