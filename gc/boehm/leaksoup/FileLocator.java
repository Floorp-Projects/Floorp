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

public class FileLocator {
	static boolean USE_BLAME = false;
	static boolean ASSIGN_BLAME = false;

	static final String MOZILLA_BASE = "/mozilla/";
	static final String LXR_BASE = "http://lxr.mozilla.org/seamonkey/source/";
	static final String BONSAI_BASE = "http://bonsai.mozilla.org/cvsblame.cgi?file=";

	static final String NS_BASE = "/ns/";
	static final String NS_BONSAI_URL = "http://warp.mcom.com/webtools/bonsai/cvsblame.cgi?root=/m/src&file=";

	static final Hashtable fileTables = new Hashtable();
	static final RevisionTable revisionTable = new RevisionTable();
	static final BlameTable blameTable = new BlameTable();
    static final Hashtable addr2LineTable = new Hashtable();

    public static String getFileLocation(byte[] line) throws IOException {
        return getFileLocation(new String(line));
    }

	public static String getFileLocation(String line) throws IOException {
        // start from the LAST occurence of '[', as C++ symbols can include it.
		int leftBracket = line.lastIndexOf('[');
		if (leftBracket == -1)
			return line;
		int rightBracket = line.indexOf(']', leftBracket + 1);
		if (rightBracket == -1)
			return line;
        
        // generalize for Linux & Mac versions of the Leak detector.
        String fullPath;
        int lineNumber, lineAnchor;
        int comma = line.indexOf(',', leftBracket + 1);
        if (comma != -1) {
            // Mac format: unmangled_symbol[mac_path,byte_offset]
            String macPath = line.substring(leftBracket + 1, comma);
            fullPath = "/" + macPath.replace(':', '/');
		
            // compute the line number in the file.
            int offset = 0;
            try {
                offset = Integer.parseInt(line.substring(comma + 1, rightBracket));
            } catch (NumberFormatException nfe) {
                return line;
            } catch (StringIndexOutOfBoundsException sobe) {
                System.err.println("### Error processing line: " + line);
                System.err.println("### comma = " + comma + ", rightBracket = " + rightBracket);
                System.err.flush();
                return line;
            }
		
            FileTable table = (FileTable) fileTables.get(fullPath);
            if (table == null) {
                table = new FileTable(fullPath);
                fileTables.put(fullPath, table);
            }
            lineNumber = 1 + table.getLine(offset);
            lineAnchor = lineNumber;
        } else {
            int space = line.indexOf(' ', leftBracket + 1);
            if (space != -1) {
                // Raw Linux format: mangled_symbol[library +address]
                String library = line.substring(leftBracket + 1, space);
                String address = line.substring(space + 1, rightBracket);
                Addr2Line addr2line = (Addr2Line) addr2LineTable.get(library);
                if (addr2line == null) {
                    System.out.println("new Addr2Line["+library+"]");
                    addr2line = new Addr2Line(library);
                    addr2LineTable.put(library, addr2line);
                }
                line = addr2line.getLine(address);
                return getFileLocation(line);
            } else {
                // Linux format: unmangled_symbol[unix_path:line_number]
                int colon = line.indexOf(':', leftBracket + 1);
                fullPath = line.substring(leftBracket + 1, colon);
                try {
                    lineNumber = Integer.parseInt(line.substring(colon + 1, rightBracket));
                } catch (NumberFormatException nfe) {
                    return line;
                }
                lineAnchor = lineNumber;
            }
        }
        
		// compute the URL of the file.
		int mozillaIndex = fullPath.indexOf(MOZILLA_BASE);
		String locationURL = null, blameInfo = "";
		if (mozillaIndex > -1) {
			// if using blame, hilite the line number of the call, and include the revision.
			String mozillaPath = fullPath.substring(mozillaIndex + 1);
			String revision = revisionTable.getRevision(fullPath);
			String bonsaiPath = mozillaPath + (revision.length() > 0 ? "&rev=" + revision : "");
			if (USE_BLAME) {
				locationURL = BONSAI_BASE + bonsaiPath + "&mark=" + lineNumber;
				if (lineAnchor > 10)
					lineAnchor -= 10;
			} else {
				locationURL = LXR_BASE + mozillaPath.substring(MOZILLA_BASE.length());
			}
			if (ASSIGN_BLAME)
				blameInfo = " (" + blameTable.getBlame(bonsaiPath, lineNumber) + ")";
		} else {
			int nsIndex = fullPath.indexOf(NS_BASE);
			if (nsIndex > -1 && USE_BLAME) {
				// if using blame, hilite the line number of the call, and include the revision.
				String nsPath = fullPath.substring(nsIndex + 1);
				String revision = revisionTable.getRevision(fullPath);
				String bonsaiPath = nsPath + (revision.length() > 0 ? "&rev=" + revision : "");
				locationURL = NS_BONSAI_URL + bonsaiPath + "&mark=" + lineNumber;
				if (lineAnchor > 10)
					lineAnchor -= 10;
				if (ASSIGN_BLAME)
					blameInfo = " (" + blameTable.getBlame(bonsaiPath, lineNumber) + ")";
			} else {
				locationURL = "file://" + fullPath;
			}
		}
		
		return "<A HREF=\"" + locationURL + "#" + lineAnchor + "\"TARGET=\"SOURCE\">" + line.substring(0, leftBracket) + "</A>" + blameInfo;
	}
}

