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

public class bloatsoup {
	public static void main(String[] args) {
		if (args.length == 0) {
			System.out.println("usage:  bloatsoup trace");
			System.exit(1);
		}
		
		for (int i = 0; i < args.length; i++) {
		    cook(args[i]);
		}
		
		// quit the application.
		System.exit(0);
	}

    /**
     * Initially, just create a histogram of the bloat.
     */
	static void cook(String inputName) {
		String outputName = inputName + ".html";

		try {
			PrintWriter writer = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputName))));
			writer.println("<HTML>");
			Date now = new Date();
			writer.println("<TITLE>Bloat as of " + now + "</TITLE>");

			int objectCount = 0;
			long totalSize = 0;
			
			Hashtable types = new Hashtable();
			Histogram hist = new Histogram();
			StringTable strings = new StringTable();
			BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(inputName)));
			String line = reader.readLine();
			while (line != null) {
				if (line.startsWith("0x")) {
					String addr = strings.intern(line.substring(0, 10));
					String name = strings.intern(line.substring(line.indexOf('<') + 1, line.indexOf('>')));
					int size;
					try {
						String str = line.substring(line.indexOf('(') + 1, line.indexOf(')')).trim();
						size = Integer.parseInt(str);
					} catch (NumberFormatException nfe) {
						size = 0;
					}
					String key = strings.intern(name + "_" + size);
					Type type = (Type) types.get(key);
					if (type == null) {
						type = new Type(name, size);
						types.put(key, type);
					}
					hist.record(type);
					
					++objectCount;
					totalSize += size;
				}
				line = reader.readLine();
			}
			reader.close();

			// print bloat summary.
			writer.println("<H2>Bloat Summary</H2>");
			writer.println("total object count = " + objectCount + "<BR>");
			writer.println("total memory bloat = " + totalSize + " bytes.<BR>");
			
            // print histogram sorted by count * size.
			writer.println("<H2>Bloat Histogram</H2>");
            leaksoup.printHistogram(writer, hist);

			writer.close();
		} catch (Exception e) {
			e.printStackTrace(System.err);
		}
	}
	
	/**
	 * Simply replaces instances of '<' with "&LT;" and '>' with "&GT;".
	 */
	static String quoteTags(String str) {
		int length = str.length();
		StringBuffer buf = new StringBuffer(length);
		for (int i = 0; i < length; ++i) {
			char ch = str.charAt(i);
			switch (ch) {
			case '<':
				buf.append("&LT;");
				break;
			case '>':
				buf.append("&GT;");
				break;
			default:
				buf.append(ch);
				break;
			}
		}
		return buf.toString();
	}
}
