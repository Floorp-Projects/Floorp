/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
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
 *   Brian Stell <bstell@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NS_FT2_FONT_CATALOG_H
#define NS_FT2_FONT_CATALOG_H

#include "nspr.h"
#include "nsHashtable.h"
#include "nsIServiceManager.h"
#include "nsCompressedCharMap.h"
#include "nsICharRepresentable.h"

#include "nsString.h"
#include "nsIPref.h"
#include "nsNameValuePairDB.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#if (defined(MOZ_ENABLE_FREETYPE2))
//
// To limit the potential for namespace collision we limit the 
// number of Moz files that include FreeType's include (and hence 
// have FreeType typedefs, macros, and defines).
//
struct FT_LibraryRec_;
typedef signed long FT_Long;
typedef unsigned short FT_UShort;

#define PUBLIC_FONT_SUMMARY_NAME  NS_LITERAL_CSTRING(".mozilla_font_summary.ndb")
#define FONT_SUMMARIES_SUBDIR     NS_LITERAL_CSTRING("catalog")
#define FONT_DOWNLOAD_SUBDIR      NS_LITERAL_CSTRING("fonts")
#define FONT_SUMMARIES_EXTENSION  NS_LITERAL_CSTRING(".ndb")
#define FONT_SUMMARY_VERSION_TAG "FontSummaryVersion"
#define FONT_SUMMARY_VERSION_MAJOR 1
#define FONT_SUMMARY_VERSION_MINOR 0
#define FONT_SUMMARY_VERSION_REV   0


#define FC_FILE_OKAY     0
#define FC_FILE_GARBLED  1
#define FC_FILE_MODIFIED 2

#define STRMATCH(s1,s2) (strcmp((s1),(s2))==0)
#define STRNMATCH(s1,s2,l) (strncmp((s1),(s2),(l))==0)
#define STRCASEMATCH(s1,s2) (strcasecmp((s1),(s2))==0)

#define FREE_IF(x) if(x) free((void*)x)

typedef struct {
  const char             *mConverterName;
  PRUint8                 mCmapPlatformID;
  PRUint8                 mCmapEncoding;
  nsIUnicodeEncoder*      mConverter;
} nsTTFontEncoderInfo;

typedef struct nsTTFontFamilyEncoderInfo {
  const char                *mFamilyName;
  nsTTFontEncoderInfo *mEncodingInfo;
} nsTTFontFamilyEncoderInfo;

typedef struct {
  const char   *mDirName; // encoded in the native charset
} nsDirCatalogEntry;


#define FCE_FLAGS_ISVALID 0x01
#define FCE_FLAGS_UNICODE 0x02
#define FCE_FLAGS_SYMBOL  0x04

typedef struct {
  const char   *mFontFileName;
  time_t        mMTime;
  PRUint32      mFlags;
  const char   *mFontType;
  int           mFaceIndex;
  int           mNumFaces;
  const char   *mFamilyName;
  const char   *mStyleName;
  FT_UShort     mWeight;
  FT_UShort     mWidth;
  int           mNumGlyphs;
  int           mNumUsableGlyphs;
  FT_Long       mFaceFlags;
  FT_Long       mStyleFlags;
  FT_Long       mCodePageRange1;
  FT_Long       mCodePageRange2;
  char          mVendorID[5];
  const char   *mFoundryName;
  int           mNumEmbeddedBitmaps;
  int          *mEmbeddedBitmapHeights;
  PRUint16     *mCCMap;       // compressed char map
} nsFontCatalogEntry;

typedef struct {
  nsFontCatalogEntry **fonts;
  int numFonts;
  int numSlots;
} nsFontCatalog;

typedef struct {
  nsDirCatalogEntry **dirs;
  int numDirs;
  int numSlots;
} nsDirCatalog;

typedef struct {
  const char *vendorID;
  const char *vendorName;
} nsFontVendorName;

typedef struct {
  unsigned long bit;
  const char *charsetName;
} nsulCodePageRangeCharSetName;

class nsFT2FontCatalog {
friend class nsFT2FontNode;
public:
  static void        FreeGlobals();
  static PRBool      InitGlobals(FT_LibraryRec_ *);
  static void        GetFontNames(const char* aPattern, nsFontCatalog* aFC);

  static PRUint16*   GetCCMap(nsFontCatalogEntry *aFce);
  static nsTTFontFamilyEncoderInfo* GetCustomEncoderInfo(const char *);
  static nsTTFontFamilyEncoderInfo* GetCustomEncoderInfo(nsFontCatalogEntry *);
  static const char* GetFileName(nsFontCatalogEntry *aFce);
  static const char* GetFamilyName(nsFontCatalogEntry *aFce);
  static PRInt32*    GetEmbeddedBitmapHeights(nsFontCatalogEntry *aFce);
  static PRInt32     GetFaceIndex(nsFontCatalogEntry *aFce);
  static PRInt32     GetNumEmbeddedBitmaps(nsFontCatalogEntry *aFce);


protected:
  void   AddDir(nsDirCatalog *dc, nsDirCatalogEntry *dir);
  PRBool AddFceIfCurrent(const char*, nsHashtable*, PRInt64, nsFontCatalog*);
  void   AddFont(nsFontCatalog *fc, nsFontCatalogEntry *fce);
  int    CheckFontSummaryVersion(nsNameValuePairDB *aDB);
#ifdef DEBUG
  void   DumpFontCatalog(nsFontCatalog *fc);
  void   DumpFontCatalogEntry(nsFontCatalogEntry *);
#endif
  void   FixUpFontCatalog(nsFontCatalog *fc);
  static PRBool FreeFceHashEntry(nsHashKey* aKey, void* aData, void* aClosure);
  void   FreeFontCatalog(nsFontCatalog *fc);
  void   FreeFontCatalogEntry(nsFontCatalogEntry *);
  void   doFreeGlobals();
  static nsICharsetConverterManager2* GetCharSetManager();
  static void GetDirsPrefEnumCallback(const char* aName, void* aClosure);
  void   doGetDirsPrefEnumCallback(const char* aName, void* aClosure);
  int    GetFontCatalog(FT_LibraryRec_*, nsFontCatalog *, nsDirCatalog *);
  void   doGetFontNames(const char* aPattern, nsFontCatalog* aFC);
  PRBool GetFontSummaryName(const nsACString &, const nsACString &,
                                   nsACString &, nsACString &);
  const char* GetFoundry(nsFontCatalogEntry *aFce);
  const char* GetRange1CharSetName(unsigned long aBit);
  const char* GetRange2CharSetName(unsigned long aBit);
  PRBool HandleFontDir(FT_LibraryRec_ *, nsFontCatalog *,
                       const nsACString &, const nsACString &);
  void   HandleFontFile(FT_LibraryRec_ *, nsFontCatalog *, const char*);
  PRBool doInitGlobals(FT_LibraryRec_ *);
  PRBool IsSpace(FT_Long);
  nsDirCatalog *NewDirCatalog();
  nsFontCatalogEntry* NewFceFromFontFile(FT_LibraryRec_*, const char*,int,int*);
  nsFontCatalogEntry* NewFceFromSummary(nsNameValuePairDB *aDB);
  nsFontCatalog *NewFontCatalog();
  PRBool ReadFontDirSummary(const nsACString&, nsHashtable*);
  PRBool ParseCCMapLine(nsCompressedCharMap*,long,const char*);
  PRBool ParseXLFD(char *, char**, char**, char**, char**);
  void   PrintCCMap(nsNameValuePairDB *aDB, PRUint16 *aCCMap);
  void   PrintFontSummaries(nsNameValuePairDB *, nsFontCatalog *);
  void   PrintFontSummaryVersion(nsNameValuePairDB *aDB);
  void   PrintPageBits(nsNameValuePairDB*,PRUint16*,PRUint32);
  int    ReadFontSummaries(nsHashtable*, nsNameValuePairDB *);

  nsCOMPtr<nsIPref> mPref;
  nsFontCatalog *mFontCatalog;
  nsHashtable   *mFontFamilies;
  PRPackedBool   mIsNewCatalog;
  nsHashtable   *mVendorNames;
  nsHashtable   *mRange1CharSetNames;
  nsHashtable   *mRange2CharSetNames;
  static nsICharsetConverterManager2* sCharSetManager;
};

extern nsFT2FontCatalog* gFT2FontCatalog;

#endif

#endif /* NS_FT2_FONT_CATALOG_H */

