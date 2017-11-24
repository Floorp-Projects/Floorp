/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  nsPluginsDirDarwin.cpp

  Mac OS X implementation of the nsPluginsDir/nsPluginsFile classes.

  by Patrick C. Beard.
 */

#include "GeckoChildProcessHost.h"
#include "base/process_util.h"

#include "prlink.h"
#include "prnetdb.h"
#include "nsXPCOM.h"

#include "nsPluginsDir.h"
#include "nsNPAPIPlugin.h"
#include "nsPluginsDirUtils.h"

#include "nsILocalFileMac.h"
#include "mozilla/UniquePtr.h"

#include "nsCocoaFeatures.h"
#include "nsExceptionHandler.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>

typedef NS_NPAPIPLUGIN_CALLBACK(const char *, NP_GETMIMEDESCRIPTION) ();
typedef NS_NPAPIPLUGIN_CALLBACK(OSErr, BP_GETSUPPORTEDMIMETYPES) (BPSupportedMIMETypes *mimeInfo, UInt32 flags);

/*
** Returns a CFBundleRef if the path refers to a Mac OS X bundle directory.
** The caller is responsible for calling CFRelease() to deallocate.
*/
static CFBundleRef getPluginBundle(const char* path)
{
  CFBundleRef bundle = nullptr;
  CFStringRef pathRef = ::CFStringCreateWithCString(nullptr, path,
                                                    kCFStringEncodingUTF8);
  if (pathRef) {
    CFURLRef bundleURL = ::CFURLCreateWithFileSystemPath(nullptr, pathRef,
                                                         kCFURLPOSIXPathStyle,
                                                         true);
    if (bundleURL) {
      bundle = ::CFBundleCreate(nullptr, bundleURL);
      ::CFRelease(bundleURL);
    }
    ::CFRelease(pathRef);
  }
  return bundle;
}

bool nsPluginsDir::IsPluginFile(nsIFile* file)
{
  nsCString fileName;
  file->GetNativeLeafName(fileName);
  /*
   * Don't load the VDP fake plugin, to avoid tripping a bad bug in OS X
   * 10.5.3 (see bug 436575).
   */
  if (!strcmp(fileName.get(), "VerifiedDownloadPlugin.plugin")) {
    NS_WARNING("Preventing load of VerifiedDownloadPlugin.plugin (see bug 436575)");
    return false;
  }
  return true;
}

// Caller is responsible for freeing returned buffer.
static char* CFStringRefToUTF8Buffer(CFStringRef cfString)
{
  const char* buffer = ::CFStringGetCStringPtr(cfString, kCFStringEncodingUTF8);
  if (buffer) {
    return PL_strdup(buffer);
  }

  int64_t bufferLength =
    ::CFStringGetMaximumSizeForEncoding(::CFStringGetLength(cfString),
                                        kCFStringEncodingUTF8) + 1;
  char* newBuffer = static_cast<char*>(moz_xmalloc(bufferLength));

  if (!::CFStringGetCString(cfString, newBuffer, bufferLength,
                            kCFStringEncodingUTF8)) {
    free(newBuffer);
    return nullptr;
  }

  newBuffer = static_cast<char*>(moz_xrealloc(newBuffer,
                                              strlen(newBuffer) + 1));
  return newBuffer;
}

class AutoCFTypeObject {
public:
  explicit AutoCFTypeObject(CFTypeRef aObject)
  {
    mObject = aObject;
  }
  ~AutoCFTypeObject()
  {
    ::CFRelease(mObject);
  }
private:
  CFTypeRef mObject;
};

static Boolean MimeTypeEnabled(CFDictionaryRef mimeDict) {
  if (!mimeDict) {
    return true;
  }

  CFTypeRef value;
  if (::CFDictionaryGetValueIfPresent(mimeDict, CFSTR("WebPluginTypeEnabled"), &value)) {
    if (value && ::CFGetTypeID(value) == ::CFBooleanGetTypeID()) {
      return ::CFBooleanGetValue(static_cast<CFBooleanRef>(value));
    }
  }
  return true;
}

static CFDictionaryRef ParsePlistForMIMETypesFilename(CFBundleRef bundle)
{
  CFTypeRef mimeFileName = ::CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginMIMETypesFilename"));
  if (!mimeFileName || ::CFGetTypeID(mimeFileName) != ::CFStringGetTypeID()) {
    return nullptr;
  }

  FSRef homeDir;
  if (::FSFindFolder(kUserDomain, kCurrentUserFolderType, kDontCreateFolder, &homeDir) != noErr) {
    return nullptr;
  }

  CFURLRef userDirURL = ::CFURLCreateFromFSRef(kCFAllocatorDefault, &homeDir);
  if (!userDirURL) {
    return nullptr;
  }

  AutoCFTypeObject userDirURLAutorelease(userDirURL);
  CFStringRef mimeFilePath = ::CFStringCreateWithFormat(kCFAllocatorDefault, nullptr, CFSTR("Library/Preferences/%@"), static_cast<CFStringRef>(mimeFileName));
  if (!mimeFilePath) {
    return nullptr;
  }

  AutoCFTypeObject mimeFilePathAutorelease(mimeFilePath);
  CFURLRef mimeFileURL = ::CFURLCreateWithFileSystemPathRelativeToBase(kCFAllocatorDefault, mimeFilePath, kCFURLPOSIXPathStyle, false, userDirURL);
  if (!mimeFileURL) {
    return nullptr;
  }

  AutoCFTypeObject mimeFileURLAutorelease(mimeFileURL);
  SInt32 errorCode = 0;
  CFDataRef mimeFileData = nullptr;
  Boolean result = ::CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, mimeFileURL, &mimeFileData, nullptr, nullptr, &errorCode);
  if (!result) {
    return nullptr;
  }

  AutoCFTypeObject mimeFileDataAutorelease(mimeFileData);
  if (errorCode != 0) {
    return nullptr;
  }

  CFPropertyListRef propertyList = ::CFPropertyListCreateFromXMLData(kCFAllocatorDefault, mimeFileData, kCFPropertyListImmutable, nullptr);
  if (!propertyList) {
    return nullptr;
  }

  AutoCFTypeObject propertyListAutorelease(propertyList);
  if (::CFGetTypeID(propertyList) != ::CFDictionaryGetTypeID()) {
    return nullptr;
  }

  CFTypeRef mimeTypes = ::CFDictionaryGetValue(static_cast<CFDictionaryRef>(propertyList), CFSTR("WebPluginMIMETypes"));
  if (!mimeTypes || ::CFGetTypeID(mimeTypes) != ::CFDictionaryGetTypeID() || ::CFDictionaryGetCount(static_cast<CFDictionaryRef>(mimeTypes)) == 0) {
    return nullptr;
  }

  return static_cast<CFDictionaryRef>(::CFRetain(mimeTypes));
}

static void ParsePlistPluginInfo(nsPluginInfo& info, CFBundleRef bundle)
{
  CFDictionaryRef mimeDict = ParsePlistForMIMETypesFilename(bundle);

  if (!mimeDict) {
    CFTypeRef mimeTypes = ::CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginMIMETypes"));
    if (!mimeTypes || ::CFGetTypeID(mimeTypes) != ::CFDictionaryGetTypeID() || ::CFDictionaryGetCount(static_cast<CFDictionaryRef>(mimeTypes)) == 0)
      return;
    mimeDict = static_cast<CFDictionaryRef>(::CFRetain(mimeTypes));
  }

  AutoCFTypeObject mimeDictAutorelease(mimeDict);
  int mimeDictKeyCount = ::CFDictionaryGetCount(mimeDict);

  // Allocate memory for mime data
  int mimeDataArraySize = mimeDictKeyCount * sizeof(char*);
  info.fMimeTypeArray = static_cast<char**>(moz_xmalloc(mimeDataArraySize));
  if (!info.fMimeTypeArray)
    return;
  memset(info.fMimeTypeArray, 0, mimeDataArraySize);
  info.fExtensionArray = static_cast<char**>(moz_xmalloc(mimeDataArraySize));
  if (!info.fExtensionArray)
    return;
  memset(info.fExtensionArray, 0, mimeDataArraySize);
  info.fMimeDescriptionArray = static_cast<char**>(moz_xmalloc(mimeDataArraySize));
  if (!info.fMimeDescriptionArray)
    return;
  memset(info.fMimeDescriptionArray, 0, mimeDataArraySize);

  // Allocate memory for mime dictionary keys and values
  mozilla::UniquePtr<CFTypeRef[]> keys(new CFTypeRef[mimeDictKeyCount]);
  if (!keys)
    return;
  mozilla::UniquePtr<CFTypeRef[]> values(new CFTypeRef[mimeDictKeyCount]);
  if (!values)
    return;

  info.fVariantCount = 0;

  ::CFDictionaryGetKeysAndValues(mimeDict, keys.get(), values.get());
  for (int i = 0; i < mimeDictKeyCount; i++) {
    CFTypeRef mimeString = keys[i];
    if (!mimeString || ::CFGetTypeID(mimeString) != ::CFStringGetTypeID()) {
      continue;
    }
    CFTypeRef mimeDict = values[i];
    if (mimeDict && ::CFGetTypeID(mimeDict) == ::CFDictionaryGetTypeID()) {
      if (!MimeTypeEnabled(static_cast<CFDictionaryRef>(mimeDict))) {
        continue;
      }
      info.fMimeTypeArray[info.fVariantCount] = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(mimeString));
      if (!info.fMimeTypeArray[info.fVariantCount]) {
        continue;
      }
      CFTypeRef extensions = ::CFDictionaryGetValue(static_cast<CFDictionaryRef>(mimeDict), CFSTR("WebPluginExtensions"));
      if (extensions && ::CFGetTypeID(extensions) == ::CFArrayGetTypeID()) {
        int extensionCount = ::CFArrayGetCount(static_cast<CFArrayRef>(extensions));
        CFMutableStringRef extensionList = ::CFStringCreateMutable(kCFAllocatorDefault, 0);
        for (int j = 0; j < extensionCount; j++) {
          CFTypeRef extension = ::CFArrayGetValueAtIndex(static_cast<CFArrayRef>(extensions), j);
          if (extension && ::CFGetTypeID(extension) == ::CFStringGetTypeID()) {
            if (j > 0)
              ::CFStringAppend(extensionList, CFSTR(","));
            ::CFStringAppend(static_cast<CFMutableStringRef>(extensionList), static_cast<CFStringRef>(extension));
          }
        }
        info.fExtensionArray[info.fVariantCount] = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(extensionList));
        ::CFRelease(extensionList);
      }
      CFTypeRef description = ::CFDictionaryGetValue(static_cast<CFDictionaryRef>(mimeDict), CFSTR("WebPluginTypeDescription"));
      if (description && ::CFGetTypeID(description) == ::CFStringGetTypeID())
        info.fMimeDescriptionArray[info.fVariantCount] = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(description));
    }
    info.fVariantCount++;
  }
}

nsPluginFile::nsPluginFile(nsIFile *spec)
    : mPlugin(spec)
{
}

nsPluginFile::~nsPluginFile() {}

nsresult nsPluginFile::LoadPlugin(PRLibrary **outLibrary)
{
  if (!mPlugin)
    return NS_ERROR_NULL_POINTER;

  // 64-bit NSPR does not (yet) support bundles.  So in 64-bit builds we need
  // (for now) to load the bundle's executable.  However this can cause
  // problems:  CFBundleCreate() doesn't run the bundle's executable's
  // initialization code, while NSAddImage() and dlopen() do run it.  So using
  // NSPR's dyld loading mechanisms here (NSAddImage() or dlopen()) can cause
  // a bundle's initialization code to run earlier than expected, and lead to
  // crashes.  See bug 577967.
#ifdef __LP64__
  char executablePath[PATH_MAX];
  executablePath[0] = '\0';
  nsAutoCString bundlePath;
  mPlugin->GetNativePath(bundlePath);
  CFStringRef pathRef = ::CFStringCreateWithCString(nullptr, bundlePath.get(),
                                                    kCFStringEncodingUTF8);
  if (pathRef) {
    CFURLRef bundleURL = ::CFURLCreateWithFileSystemPath(nullptr, pathRef,
                                                         kCFURLPOSIXPathStyle,
                                                         true);
    if (bundleURL) {
      CFBundleRef bundle = ::CFBundleCreate(nullptr, bundleURL);
      if (bundle) {
        CFURLRef executableURL = ::CFBundleCopyExecutableURL(bundle);
        if (executableURL) {
          if (!::CFURLGetFileSystemRepresentation(executableURL, true, (UInt8*)&executablePath, PATH_MAX))
            executablePath[0] = '\0';
          ::CFRelease(executableURL);
        }
        ::CFRelease(bundle);
      }
      ::CFRelease(bundleURL);
    }
    ::CFRelease(pathRef);
  }
#else
  nsAutoCString bundlePath;
  mPlugin->GetNativePath(bundlePath);
  const char *executablePath = bundlePath.get();
#endif

  *outLibrary = PR_LoadLibrary(executablePath);
  pLibrary = *outLibrary;
  if (!pLibrary) {
    return NS_ERROR_FAILURE;
  }
#ifdef DEBUG
  printf("[loaded plugin %s]\n", bundlePath.get());
#endif
  return NS_OK;
}

static char* p2cstrdup(StringPtr pstr)
{
  int len = pstr[0];
  char* cstr = static_cast<char*>(moz_xmalloc(len + 1));
  if (cstr) {
    memmove(cstr, pstr + 1, len);
    cstr[len] = '\0';
  }
  return cstr;
}

static char* GetNextPluginStringFromHandle(Handle h, short *index)
{
  char *ret = p2cstrdup((unsigned char*)(*h + *index));
  *index += (ret ? strlen(ret) : 0) + 1;
  return ret;
}

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info, PRLibrary **outLibrary)
{
  *outLibrary = nullptr;

  nsresult rv = NS_OK;

  // clear out the info, except for the first field.
  memset(&info, 0, sizeof(info));

  // Try to get a bundle reference.
  nsAutoCString path;
  if (NS_FAILED(rv = mPlugin->GetNativePath(path)))
    return rv;
  CFBundleRef bundle = getPluginBundle(path.get());

  // fill in full path
  info.fFullPath = PL_strdup(path.get());

  // fill in file name
  nsAutoCString fileName;
  if (NS_FAILED(rv = mPlugin->GetNativeLeafName(fileName)))
    return rv;
  info.fFileName = PL_strdup(fileName.get());

  // Get fName
  if (bundle) {
    CFTypeRef name = ::CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginName"));
    if (name && ::CFGetTypeID(name) == ::CFStringGetTypeID())
      info.fName = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(name));
  }

  // Get fDescription
  if (bundle) {
    CFTypeRef description = ::CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginDescription"));
    if (description && ::CFGetTypeID(description) == ::CFStringGetTypeID())
      info.fDescription = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(description));
  }

  // Get fVersion
  if (bundle) {
    // Look for the release version first
    CFTypeRef version = ::CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleShortVersionString"));
    if (!version) // try the build version
      version = ::CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleVersionKey);
    if (version && ::CFGetTypeID(version) == ::CFStringGetTypeID())
      info.fVersion = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(version));
  }

  // The last thing we need to do is get MIME data
  // fVariantCount, fMimeTypeArray, fExtensionArray, fMimeDescriptionArray

  // First look for data in a bundle plist
  if (bundle) {
    ParsePlistPluginInfo(info, bundle);
    ::CFRelease(bundle);
    if (info.fVariantCount > 0)
      return NS_OK;
  }

  // Don't load "fbplugin" or any plugins whose name starts with "fbplugin_"
  // (Facebook plugins) if we're running on OS X 10.10 (Yosemite) or later.
  // A "fbplugin" file crashes on load, in the call to LoadPlugin() below.
  // See bug 1086977.
  if (nsCocoaFeatures::OnYosemiteOrLater()) {
    if (fileName.EqualsLiteral("fbplugin") ||
        StringBeginsWith(fileName, NS_LITERAL_CSTRING("fbplugin_"))) {
      nsAutoCString msg;
      msg.AppendPrintf("Preventing load of %s (see bug 1086977)",
                       fileName.get());
      NS_WARNING(msg.get());
      return NS_ERROR_FAILURE;
    }

    // The block above assumes that "fbplugin" is the filename of the plugin
    // to be blocked, or that the filename starts with "fbplugin_".  But we
    // don't yet know for sure if this is always true.  So for the time being
    // record extra information in our crash logs.
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("Bug_1086977"),
                                       fileName);
  }

  // It's possible that our plugin has 2 entry points that'll give us mime type
  // info. Quicktime does this to get around the need of having admin rights to
  // change mime info in the resource fork. We need to use this info instead of
  // the resource. See bug 113464.

  // Sadly we have to load the library for this to work.
  rv = LoadPlugin(outLibrary);

  if (nsCocoaFeatures::OnYosemiteOrLater()) {
    // If we didn't crash in LoadPlugin(), change the previous annotation so we
    // don't sow confusion.
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("Bug_1086977"),
                                       NS_LITERAL_CSTRING("Didn't crash, please ignore"));
  }

  if (NS_FAILED(rv))
    return rv;

  // Try to get data from NP_GetMIMEDescription
  if (pLibrary) {
    NP_GETMIMEDESCRIPTION pfnGetMimeDesc = (NP_GETMIMEDESCRIPTION)PR_FindFunctionSymbol(pLibrary, NP_GETMIMEDESCRIPTION_NAME);
    if (pfnGetMimeDesc)
      ParsePluginMimeDescription(pfnGetMimeDesc(), info);
    if (info.fVariantCount)
      return NS_OK;
  }

  // We'll fill this in using BP_GetSupportedMIMETypes and/or resource fork data
  BPSupportedMIMETypes mi = {kBPSupportedMIMETypesStructVers_1, nullptr, nullptr};

  // Try to get data from BP_GetSupportedMIMETypes
  if (pLibrary) {
    BP_GETSUPPORTEDMIMETYPES pfnMime = (BP_GETSUPPORTEDMIMETYPES)PR_FindFunctionSymbol(pLibrary, "BP_GetSupportedMIMETypes");
    if (pfnMime && noErr == pfnMime(&mi, 0) && mi.typeStrings) {
      info.fVariantCount = (**(short**)mi.typeStrings) / 2;
      ::HLock(mi.typeStrings);
      if (mi.infoStrings)  // it's possible some plugins have infoStrings missing
        ::HLock(mi.infoStrings);
    }
  }

  // Fill in the info struct based on the data in the BPSupportedMIMETypes struct
  int variantCount = info.fVariantCount;
  info.fMimeTypeArray = static_cast<char**>(moz_xmalloc(variantCount * sizeof(char*)));
  if (!info.fMimeTypeArray)
    return NS_ERROR_OUT_OF_MEMORY;
  info.fExtensionArray = static_cast<char**>(moz_xmalloc(variantCount * sizeof(char*)));
  if (!info.fExtensionArray)
    return NS_ERROR_OUT_OF_MEMORY;
  if (mi.infoStrings) {
    info.fMimeDescriptionArray = static_cast<char**>(moz_xmalloc(variantCount * sizeof(char*)));
    if (!info.fMimeDescriptionArray)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  short mimeIndex = 2;
  short descriptionIndex = 2;
  for (int i = 0; i < variantCount; i++) {
    info.fMimeTypeArray[i] = GetNextPluginStringFromHandle(mi.typeStrings, &mimeIndex);
    info.fExtensionArray[i] = GetNextPluginStringFromHandle(mi.typeStrings, &mimeIndex);
    if (mi.infoStrings)
      info.fMimeDescriptionArray[i] = GetNextPluginStringFromHandle(mi.infoStrings, &descriptionIndex);
  }

  ::HUnlock(mi.typeStrings);
  ::DisposeHandle(mi.typeStrings);
  if (mi.infoStrings) {
    ::HUnlock(mi.infoStrings);
    ::DisposeHandle(mi.infoStrings);
  }

  return NS_OK;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  free(info.fName);
  free(info.fDescription);
  int variantCount = info.fVariantCount;
  for (int i = 0; i < variantCount; i++) {
    free(info.fMimeTypeArray[i]);
    free(info.fExtensionArray[i]);
    free(info.fMimeDescriptionArray[i]);
  }
  free(info.fMimeTypeArray);
  free(info.fMimeDescriptionArray);
  free(info.fExtensionArray);
  free(info.fFileName);
  free(info.fFullPath);
  free(info.fVersion);

  return NS_OK;
}
