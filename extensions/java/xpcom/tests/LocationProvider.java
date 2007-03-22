/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

import org.mozilla.xpcom.IAppFileLocProvider;


public class LocationProvider implements IAppFileLocProvider {

	private File libXULPath;
	private File profile;
	private File history;
	
	public LocationProvider(File aBinPath, File aProfileDir)
	throws IOException {
		libXULPath = aBinPath;
		profile = aProfileDir;

		if (!libXULPath.exists() || !libXULPath.isDirectory()) {
			throw new FileNotFoundException("libxul directory specified is not valid: "
					+ libXULPath.getAbsolutePath());
		}
		if (profile != null && (!profile.exists() || !profile.isDirectory())) {
			throw new FileNotFoundException("profile directory specified is not valid: "
					+ profile.getAbsolutePath());
		}

		// create history file
		if (profile != null) {
			setupProfile();
		}
	}
	
	private void setupProfile() throws IOException {
		history = new File(profile, "history.dat");
		if (!history.exists()) {
			history.createNewFile();
		}
	}

	public File getFile(String aProp, boolean[] aPersistent) {
		File file = null;
		if (aProp.equals("GreD") || aProp.equals("GreComsD")) {
			file = libXULPath;
			if (aProp.equals("GreComsD")) {
				file = new File(file, "components");
			}
		}
		else if (aProp.equals("MozBinD") || 
			aProp.equals("CurProcD") ||
			aProp.equals("ComsD")) 
		{
			file = libXULPath;
			if (aProp.equals("ComsD")) {
				file = new File(file, "components");
			}
		}
		else if (aProp.equals("ProfD")) {
			return profile;
		}
		else if (aProp.equals("UHist")) {
			return history;
		}

		return file;
	}

	public File[] getFiles(String aProp) {
		File[] files = null;
		if (aProp.equals("APluginsDL")) {
			files = new File[1];
			files[0] = new File(libXULPath, "plugins");
		}

		return files;
	}

}
