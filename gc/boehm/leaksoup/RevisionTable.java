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

/**
 * Provides a way to map from a file path to its CVS revision number.
 */
public class RevisionTable {
	private Hashtable revisions = new Hashtable();

	public String getRevision(String path) throws IOException {
		String revision = (String) revisions.get(path);
		if (revision == null) {
			int lastSlash = path.lastIndexOf('/');
			String dirPath = path.substring(0, lastSlash + 1);
			if (!readEntries(dirPath))
				revisions.put(path, "");
			revision = (String) revisions.get(path);
		}
		return revision;
	}
	
	/**
	 * Reads all of the entries from a CVS Entries file, and populates
	 * the hashtable with the revisions, keyed by file paths.
	 */
	private boolean readEntries(String dirPath) throws IOException {
		File entriesFile = new File(dirPath + "CVS/Entries");
		if (entriesFile.exists()) {		
			BufferedReader entries = new BufferedReader(new InputStreamReader(new FileInputStream(entriesFile)));
			for (String line = entries.readLine(); line != null; line = entries.readLine()) {
				if (line.charAt(0) == '/') {
					int secondSlash = line.indexOf('/', 1);
					String fileName = line.substring(1, secondSlash);
					int thirdSlash = line.indexOf('/', secondSlash + 1);
					String revision = line.substring(secondSlash + 1, thirdSlash);
					revisions.put(dirPath + fileName, revision);
				}
			}
			entries.close();
			return true;
		}
		return false;
	}
}

