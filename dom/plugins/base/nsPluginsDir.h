/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
	uint32_t fVariantCount;
	char** fMimeTypeArray;
	char** fMimeDescriptionArray;
	char** fExtensionArray;
	char* fFileName;
	char* fFullPath;
	char* fVersion;
	bool fSupportsAsyncRender;
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
	explicit nsPluginFile(nsIFile* spec);
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
