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

/**
 * Provides a way to map from byte offsets into a file to line numbers.
 */
public class FileTable {
	private static class Line {
		int mOffset;
		int mLength;
		
		Line(int offset, int length) {
			mOffset = offset;
			mLength = length;
		}
	}
	private Line[] mLines;

	public FileTable(String path) throws IOException {
		Vector lines = new Vector();
		BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(path)));
		int offset = 0;
		for (String line = reader.readLine(); line != null; line = reader.readLine()) {
			// always add 1 for the line feed.
			int length = 1 + line.length();
			lines.addElement(new Line(offset, length));
			offset += length;
		}
		reader.close();
		int size = lines.size();
		mLines = new Line[size];
		lines.copyInto(mLines);
	}
	
	public int getLine(int offset) {
		// use binary search to find the line which spans this offset.
		int length = mLines.length;
		int minIndex = 0, maxIndex = length - 1;
		int index = maxIndex / 2;
		while (minIndex <= maxIndex) {
			Line line = mLines[index];
			if (offset < line.mOffset) {
				maxIndex = (index - 1);
				index = (minIndex + maxIndex) / 2;
			} else {
				if (offset < (line.mOffset + line.mLength)) {
					return index;
				}
				minIndex = (index + 1);
				index = (minIndex + maxIndex) / 2;
			}
		}
		// this case shouldn't happen, but provides a helpful value to detect errors.
		return -1;
	}
}
