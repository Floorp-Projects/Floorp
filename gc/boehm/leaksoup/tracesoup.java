/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

import java.io.*;
import java.util.*;

public class tracesoup {
	public static void main(String[] args) {
		if (args.length == 0) {
			System.out.println("usage:  tracesoup [-blame] trace");
			System.exit(1);
		}
		
		for (int i = 0; i < args.length; i++) {
			String arg = args[i];
			if (arg.charAt(0) == '-') {
				if (arg.equals("-blame"))
					FileLocator.USE_BLAME = true;
			} else {
				cook(arg);
			}
		}
		
		// quit the application.
		System.exit(0);
	}

	static void cook(String inputName) {
		String outputName = inputName + ".html";

		try {
			PrintWriter writer = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputName))));
			writer.println("<TITLE>" + outputName + "</TITLE>");
			writer.println("<PRE>");
			
			Hashtable fileTables = new Hashtable();
			BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(inputName)));
			String line = reader.readLine();
			while (line != null) {
				if (line.length() > 0) {
					// have to quote '<' & '>' characters.
					if (line.charAt(0) == '<')
						line = quoteTags(line);
					// lines containing square brackets need to be wrapped with <A HREF="..."></A>.
					int leftBracket = line.indexOf('[');
					if (leftBracket >= 0)
						line = FileLocator.getFileLocation(line);
				}
				writer.println(line);
				line = reader.readLine();
			}
			reader.close();
			
			writer.println("</PRE>");
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
