/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

import java.io.*;
import java.util.*;
import java.net.*;

class Blame {
	String[] lines;
	Hashtable info;
	
	Blame(String[] lines, Hashtable info) {
		this.lines = lines;
		this.info = info;
	}
}

public class BlameTable {
	private Hashtable blameTable = new Hashtable();
	
	public String getBlame(String bonsaiPath, int line) {
		Blame blame = (Blame) blameTable.get(bonsaiPath);
		if (blame == null) {
			blame = assignBlame(bonsaiPath);
			blameTable.put(bonsaiPath, blame);
		}
		if (blame != null) {
			String[] lines = blame.lines;
			if (line > 0 && lines.length > 0 && line <= lines.length)
				return (String) blame.info.get(lines[line - 1]);
		}
		return "error";
	}
	
	static final String CVSBLAME_CGI = "http://bonsai.mozilla.org/cvsblame.cgi?data=1&file=";

	private static int parseInt(String str) {
		int value = 0;
		try {
			value = Integer.parseInt(str);
		} catch (NumberFormatException nfe) {
		}
		return value;
	}

	/**
	 * Simply replaces instances of '<' with "&LT;" and '>' with "&GT;".
	 */
	static String quoteTags(StringBuffer buf) {
		int length = buf.length();
		for (int i = 0; i < length; ++i) {
			char ch = buf.charAt(i);
			switch (ch) {
			case '<':
				buf.setCharAt(i, '&');
				buf.insert(i + 1, "LT;");
				i += 4;
				length += 3;
				break;
			case '>':
				buf.setCharAt(i, '&');
				buf.insert(i + 1, "GT;");
				i += 4;
				length += 3;
				break;
			}
		}
		return buf.toString();
	}

	/**
	 * Contact bonsai, and get a blame table for each line in a file.
	 * This information can be compressed, in all likelyhood.
	 */
	private Blame assignBlame(String bonsaiPath) {
		try {
			Vector vec = new Vector();
			URL url = new URL(CVSBLAME_CGI + bonsaiPath);
			BufferedReader reader = new BufferedReader(new InputStreamReader(url.openStream()));
			
			// read revision for each line of the file. this is really slow. I asked slam to
			// use buffered output, to speed this up.
			for (String line = reader.readLine(); line != null; line = reader.readLine()) {
				if (line.charAt(0) == 'R')
					break;
				// decompress "revision:count" to simplify getBlame() above.
				int colonIndex = line.indexOf(':');
				String revision = line.substring(0, colonIndex);
				int count = parseInt(line.substring(colonIndex + 1));
				while (count-- > 0)
					vec.addElement(revision);
			}
			String[] lines = new String[vec.size()];
			vec.copyInto(lines);
			vec.removeAllElements();

			// read revision records, which are of the form "rev|date|who|comment" where comment can span multiple lines,
			// and each record is terminated by a "." on its own line.
			Hashtable info = new Hashtable();
			String revision = null;
			StringBuffer buffer = new StringBuffer();
			for (String line = reader.readLine(); line != null; line = reader.readLine()) {
				// we're in one of two states, either we've seen a line starting with a revision,
				// and we're waiting for the final ending line, or we're starting a new revision
				// record.
				if (revision != null) {
					if (line.equals(".")) {
						// end of current revision record.
						info.put(revision, quoteTags(buffer));
						revision = null;
						buffer.setLength(0);
					} else {
						// continuation of a comment
						buffer.append(" " + line);
					}
				} else {
					int orIndex = line.indexOf('|');
					if (orIndex >= 0) {
						// new revision info record.
						revision = line.substring(0, orIndex);
						buffer.append(line);
					}
				}
			}
			reader.close();
			
			return new Blame(lines, info);
		} catch (Exception e) {
			System.err.println("[error assigning blame for: " + bonsaiPath + "]"); 
			e.printStackTrace(System.err);
		}
		return null;
	}
}
