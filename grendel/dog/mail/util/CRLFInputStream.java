/*
 * CRLFInputStream.java
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
 * An input stream that filters out CR/LF pairs into LFs.
 *
 * @author dog@dog.net.uk
 * @version 0.1
 */
public class CRLFInputStream extends PushbackInputStream {

	/**
	 * The CR octet.
	 */
	public static final int CR = 13;
	
	/**
	 * The LF octet.
	 */
	public static final int LF = 10;
	
	/**
	 * Constructs a CR/LF input stream connected to the specified input stream.
	 */
	public CRLFInputStream(InputStream in) {
		super(in);
	}

	/**
	 * Reads the next byte of data from this input stream.
	 * Returns -1 if the end of the stream has been reached.
	 * @exception IOException if an I/O error occurs
	 */
	public int read() throws IOException {
		int c = super.read();
		if (c==CR) // skip CR
			return super.read();
		return c;
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
		int l = super.read(b, off, len);
		return removeCRs(b, off, l);
	}

	/**
	 * Reads a line of input terminated by LF.
	 * @exception IOException if an I/O error occurs
	 */
	public String readLine() throws IOException {
		byte[] acc = new byte[4096];
		int count = 0;
		for (byte b=(byte)read(); b!=LF && b!=-1; b=(byte)read())
			acc[count++] = b;
		if (count>0) {
			byte[] bytes = new byte[count];
			System.arraycopy(acc, 0, bytes, 0, count);
			return new String(bytes);
		} else
			return null;
	}

	int removeCRs(byte[] b, int off, int len) {
		for (int index = indexOfCR(b, off, len); index>-1; index = indexOfCR(b, off, len)) {
			for (int i=index; i<b.length-1; i++)
				b[i] = b[i+1];
			len--;
		}
		return len;
	}

	int indexOfCR(byte[] b, int off, int len) {
		for (int i=off; i<off+len; i++)
			if (b[i]==CR) return i;
		return -1;
	}
	
}