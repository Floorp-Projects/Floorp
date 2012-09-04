/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// OS/2 plugin-loading code.

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "nsPluginsDir.h"
#include "prlink.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "npapi.h"

#include "nsString.h"

/* Load a string stored as RCDATA in a resource segment */
/* Returned string needs to be PR_Free'd by caller */
static char *LoadRCDATAString( HMODULE hMod, ULONG resid)
{
   APIRET rc;
   ULONG  ulSize = 0;
   char  *string = 0;

   rc = DosQueryResourceSize( hMod, RT_RCDATA, resid, &ulSize);

   if( rc == NO_ERROR)
   {
      char *readOnlyString = 0;
      rc = DosGetResource( hMod, RT_RCDATA, resid, (void**) &readOnlyString);

      /* allow for 0-termination if user hasn't got it right */
      if( readOnlyString[ ulSize - 1] != '\0')
         ulSize++;

      if( rc == NO_ERROR)
      {
         /* copy string & zero-terminate */
         string = (char*) PR_Malloc( ulSize);
         memcpy( string, readOnlyString, ulSize - 1);
         string[ ulSize - 1] = '\0';

         DosFreeResource( readOnlyString);
      }
   }

   return string;
}

/* Load a version string stored as RCDATA in a resource segment */
/* Returned string needs to be PR_Free'd by caller */
static char *LoadRCDATAVersion(HMODULE hMod, ULONG resid)
{
   APIRET rc;
   ULONG  ulSize = 0;
   char  *string = 0;

   rc = DosQueryResourceSize(hMod, RT_RCDATA, resid, &ulSize);

   // version info is should be 8 chars
   if (rc == NO_ERROR && ulSize == 8)
   {
      char *version = NULL;
      rc = DosGetResource(hMod, RT_RCDATA, resid, (void**) &version);

      if (rc == NO_ERROR)
      {
         string = PR_smprintf("%d.%d.%d.%d\n",
                              version[0], version[2], version[4], version[6]);

         DosFreeResource(version);
      }
   }

   return string;
}

static uint32_t CalculateVariantCount(char* mimeTypes)
{
  uint32_t variants = 1;

  if(mimeTypes == nullptr)
    return 0;

  char* index = mimeTypes;
  while (*index)
  {
    if (*index == '|')
    variants++;

    ++index;
  }
  return variants;
}

static char** MakeStringArray(uint32_t variants, char* data)
{
  if((variants <= 0) || (data == nullptr))
    return nullptr;

  char ** array = (char **)PR_Calloc(variants, sizeof(char *));
  if(array == nullptr)
    return nullptr;

  char * start = data;
  for(uint32_t i = 0; i < variants; i++)
  {
    char * p = PL_strchr(start, '|');
    if(p != nullptr)
      *p = 0;

    array[i] = PL_strdup(start);
    start = ++p;
  }
  return array;
}

static void FreeStringArray(uint32_t variants, char ** array)
{
  if((variants == 0) || (array == nullptr))
    return;

  for(uint32_t i = 0; i < variants; i++)
  {
    if(array[i] != nullptr)
    {
      PL_strfree(array[i]);
      array[i] = nullptr;
    }
  }
  PR_Free(array);
}

// nsPluginsDir class

bool nsPluginsDir::IsPluginFile(nsIFile* file)
{
    nsAutoCString leaf;
    if (NS_FAILED(file->GetNativeLeafName(leaf)))
        return false;

    const char *leafname = leaf.get();
    
    if( nullptr != leafname)
    {
      int len = strlen( leafname);
      if( len > 6 &&                 // np*.dll
          (0 == strnicmp( &(leafname[len - 4]), ".dll", 4)) &&
          (0 == strnicmp( leafname, "np", 2)))
      {
        return true;
      }
    }
    return false;
}

// nsPluginFile implementation

nsPluginFile::nsPluginFile(nsIFile* file)
: mPlugin(file)
{}

nsPluginFile::~nsPluginFile()
{}

// Loads the plugin into memory using NSPR's shared-library loading
nsresult nsPluginFile::LoadPlugin(PRLibrary **outLibrary)
{
    if (!mPlugin)
      return NS_ERROR_NULL_POINTER;
   
    nsAutoCString temp;
    mPlugin->GetNativePath(temp);

    *outLibrary = PR_LoadLibrary(temp.get());
    return *outLibrary == nullptr ? NS_ERROR_FAILURE : NS_OK;
}

// Obtains all of the information currently available for this plugin.
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo &info, PRLibrary **outLibrary)
{
   *outLibrary = nullptr;

   nsresult   rv = NS_ERROR_FAILURE;
   HMODULE    hPlug = 0; // Need a HMODULE to query resource statements
   char       failure[ CCHMAXPATH] = "";
   APIRET     ret;

   nsAutoCString path;
   if (NS_FAILED(rv = mPlugin->GetNativePath(path)))
     return rv;

   nsAutoCString fileName;
   if (NS_FAILED(rv = mPlugin->GetNativeLeafName(fileName)))
     return rv;

   ret = DosLoadModule( failure, CCHMAXPATH, path.get(), &hPlug);
   info.fVersion = nullptr;

   while( ret == NO_ERROR)
   {
      info.fName = LoadRCDATAString( hPlug, NP_INFO_ProductName);

      info.fVersion = LoadRCDATAVersion( hPlug, NP_INFO_ProductVersion);

      // get description (doesn't matter if it's missing)...
      info.fDescription = LoadRCDATAString( hPlug, NP_INFO_FileDescription);

      char * mimeType = LoadRCDATAString( hPlug, NP_INFO_MIMEType);
      if( nullptr == mimeType) break;

      char * mimeDescription = LoadRCDATAString( hPlug, NP_INFO_FileOpenName);
      if( nullptr == mimeDescription) break;

      char * extensions = LoadRCDATAString( hPlug, NP_INFO_FileExtents);
      if( nullptr == extensions) break;

      info.fVariantCount = CalculateVariantCount(mimeType);

      info.fMimeTypeArray = MakeStringArray(info.fVariantCount, mimeType);
      if( info.fMimeTypeArray == nullptr) break;

      info.fMimeDescriptionArray = MakeStringArray(info.fVariantCount, mimeDescription);
      if( nullptr == info.fMimeDescriptionArray) break;

      info.fExtensionArray = MakeStringArray(info.fVariantCount, extensions);
      if( nullptr == info.fExtensionArray) break;

      info.fFullPath = PL_strdup(path.get());
      info.fFileName = PL_strdup(fileName.get());

      rv = NS_OK;
      break;
   }

   if( 0 != hPlug)
      DosFreeModule( hPlug);

   return rv;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
   if(info.fName != nullptr)
     PL_strfree(info.fName);

   if(info.fFullPath != nullptr)
     PL_strfree(info.fFullPath);

   if(info.fFileName != nullptr)
     PL_strfree(info.fFileName);
 
   if(info.fVersion != nullptr)
     PL_strfree(info.fVersion);
 
   if(info.fDescription != nullptr)
     PL_strfree(info.fDescription);
 
   if(info.fMimeTypeArray != nullptr)
     FreeStringArray(info.fVariantCount, info.fMimeTypeArray);
 
   if(info.fMimeDescriptionArray != nullptr)
     FreeStringArray(info.fVariantCount, info.fMimeDescriptionArray);
 
   if(info.fExtensionArray != nullptr)
     FreeStringArray(info.fVariantCount, info.fExtensionArray);
 
   memset((void *)&info, 0, sizeof(info));
 
   return NS_OK;
}
