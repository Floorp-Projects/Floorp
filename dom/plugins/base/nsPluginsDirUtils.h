/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginsDirUtils_h___
#define nsPluginsDirUtils_h___

#include "nsPluginsDir.h"
#include "nsTArray.h"

///////////////////////////////////////////////////////////////////////////////
// Output format from NPP_GetMIMEDescription: "...mime
// type[;version]:[extension]:[desecription];..." The ambiguity of mime
// description could cause the browser fail to parse the MIME types correctly.
// E.g. "mime type::desecription;" // correct w/o ext
//      "mime type:desecription;"  // wrong w/o ext
//
static nsresult ParsePluginMimeDescription(const char* mdesc,
                                           nsPluginInfo& info) {
  nsresult rv = NS_ERROR_FAILURE;
  if (!mdesc || !*mdesc) return rv;

  char* mdescDup =
      PL_strdup(mdesc);  // make a dup of intput string we'll change it content
  char anEmptyString[] = "";
  AutoTArray<char*, 8> tmpMimeTypeArr;
  char delimiters[] = {':', ':', ';'};
  int mimeTypeVariantCount = 0;
  char* p = mdescDup;  // make a dup of intput string we'll change it content
  while (p) {
    char* ptrMimeArray[] = {anEmptyString, anEmptyString, anEmptyString};

    // It's easy to point out ptrMimeArray[0] to the string sounds like
    // "Mime type is not specified, plugin will not function properly."
    // and show this on "about:plugins" page, but we have to mark this
    // particular mime type of given plugin as disable on "about:plugins" page,
    // which feature is not implemented yet.
    // So we'll ignore, without any warnings, an empty description strings,
    // in other words, if after parsing ptrMimeArray[0] == anEmptyString is
    // true. It is possible do not to registry a plugin at all if it returns an
    // empty string on GetMIMEDescription() call, e.g. plugger returns "" if
    // pluggerrc file is not found.

    char* s = p;
    int i;
    for (i = 0;
         i < (int)sizeof(delimiters) && (p = PL_strchr(s, delimiters[i]));
         i++) {
      ptrMimeArray[i] = s;  // save start ptr
      *p++ = 0;             // overwrite delimiter
      s = p;                // move forward
    }
    if (i == 2) ptrMimeArray[i] = s;
    // fill out the temp array
    // the order is important, it should be the same in for loop below
    if (ptrMimeArray[0] != anEmptyString) {
      tmpMimeTypeArr.AppendElement(ptrMimeArray[0]);
      tmpMimeTypeArr.AppendElement(ptrMimeArray[1]);
      tmpMimeTypeArr.AppendElement(ptrMimeArray[2]);
      mimeTypeVariantCount++;
    }
  }

  // fill out info structure
  if (mimeTypeVariantCount) {
    info.fVariantCount = mimeTypeVariantCount;
    // we can do these 3 mallocs at once, later on code cleanup
    info.fMimeTypeArray = (char**)malloc(mimeTypeVariantCount * sizeof(char*));
    info.fMimeDescriptionArray =
        (char**)malloc(mimeTypeVariantCount * sizeof(char*));
    info.fExtensionArray = (char**)malloc(mimeTypeVariantCount * sizeof(char*));

    int j, i;
    for (j = i = 0; i < mimeTypeVariantCount; i++) {
      // the order is important, do not change it
      // we can get rid of PL_strdup here, later on code cleanup
      info.fMimeTypeArray[i] = PL_strdup(tmpMimeTypeArr.ElementAt(j++));
      info.fExtensionArray[i] = PL_strdup(tmpMimeTypeArr.ElementAt(j++));
      info.fMimeDescriptionArray[i] = PL_strdup(tmpMimeTypeArr.ElementAt(j++));
    }
    rv = NS_OK;
  }
  if (mdescDup) PL_strfree(mdescDup);
  return rv;
}

#endif /* nsPluginsDirUtils_h___ */
