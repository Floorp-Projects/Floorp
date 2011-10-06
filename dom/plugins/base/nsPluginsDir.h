/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsPluginsDir_h_
#define nsPluginsDir_h_

#include "nsError.h"
#include "nsIFile.h"

/**
 * nsPluginsDir is nearly obsolete. Directory Service should be used instead.
 * It exists for the sake of one static function.
 */

class nsPluginsDir {
public:
	/**
	 * Determines whether or not the given file is actually a plugin file.
	 */
	static bool IsPluginFile(nsIFile* file);
};

struct PRLibrary;

struct nsPluginInfo {
	char* fName;				// name of the plugin
	char* fDescription;			// etc.
	PRUint32 fVariantCount;
	char** fMimeTypeArray;
	char** fMimeDescriptionArray;
	char** fExtensionArray;
	char* fFileName;
	char* fFullPath;
	char* fVersion;
};

/**
 * Provides cross-platform access to a plugin file. Deals with reading
 * properties from the plugin file, and loading the plugin's shared
 * library. Insulates core nsIPluginHost implementations from these
 * details.
 */
class nsPluginFile {
  PRLibrary* pLibrary;
  nsCOMPtr<nsIFile> mPlugin;
public:
	/**
	 * If spec corresponds to a valid plugin file, constructs a reference
	 * to a plugin file on disk. Plugins are typically located using the
	 * nsPluginsDir class.
	 */
	nsPluginFile(nsIFile* spec);
	virtual ~nsPluginFile();

	/**
	 * Loads the plugin into memory using NSPR's shared-library loading
	 * mechanism. Handles platform differences in loading shared libraries.
	 */
	nsresult LoadPlugin(PRLibrary **outLibrary);

	/**
	 * Obtains all of the information currently available for this plugin.
	 * Has a library outparam which will be non-null if a library load was required.
	 */
	nsresult GetPluginInfo(nsPluginInfo &outPluginInfo, PRLibrary **outLibrary);

  /**
	 * Should be called after GetPluginInfo to free all allocated stuff
	 */
	nsresult FreePluginInfo(nsPluginInfo &PluginInfo);
};

#endif /* nsPluginsDir_h_ */
