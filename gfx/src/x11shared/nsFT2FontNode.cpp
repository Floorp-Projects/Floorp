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
 *   Louie Zhao  <louie.zhao@sun.com>
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

#include "nsFT2FontNode.h"
#include "nsFreeType.h"
#include "nsFontDebug.h"
#include "nsFontFreeType.h"

#if (!defined(MOZ_ENABLE_FREETYPE2))

void nsFT2FontNode::FreeGlobals() {};

nsresult nsFT2FontNode::InitGlobals()
{
  FREETYPE_FONT_PRINTF(("freetype not compiled in"));
  NS_WARNING("freetype not compiled in");
  return NS_OK;
}

void nsFT2FontNode::GetFontNames(const char* aPattern,
                                 nsFontNodeArray* aNodes) {};

#else

nsHashtable* nsFT2FontNode::mFreeTypeNodes = nsnull;
PRBool       nsFT2FontNode::sInited = PR_FALSE;

void
nsFT2FontNode::FreeGlobals()
{
  if (mFreeTypeNodes) {
    mFreeTypeNodes->Reset(FreeNode, nsnull);
    delete mFreeTypeNodes;
    mFreeTypeNodes = nsnull;
  }
  nsFreeTypeFreeGlobals();
  sInited = PR_FALSE;
}

nsresult
nsFT2FontNode::InitGlobals()
{
  NS_ASSERTION(sInited==PR_FALSE,
               "nsFT2FontNode::InitGlobals called more than once");
  
  sInited = PR_TRUE;

  nsresult rv;
  rv = nsFreeTypeInitGlobals();
  if (NS_FAILED(rv))
    return rv;

  if (!gFT2FontCatalog)
    return NS_ERROR_FAILURE;

  mFreeTypeNodes = new nsHashtable();
  if (!mFreeTypeNodes)
    return NS_ERROR_FAILURE;
  
  if (gFT2FontCatalog->mFontCatalog) {
    LoadNodeTable(gFT2FontCatalog->mFontCatalog);
  }
  //setup the weighting table
  WeightTableInitCorrection(nsFreeTypeFont::sLinearWeightTable,
                            nsFreeType::gAATTDarkTextMinValue,
                            nsFreeType::gAATTDarkTextGain);
  
  return NS_OK;
}

void
nsFT2FontNode::GetFontNames(const char* aPattern, nsFontNodeArray* aNodes)
{
  int i, j;
  PRBool rslt;
  char *pattern, *foundry, *family, *charset, *encoding;
  const char *charSetName;
  nsFontNode *node;
  nsFontCatalog * aFC;

  if (!gFT2FontCatalog) return;

  FONT_CATALOG_PRINTF(("looking for FreeType font matching %s", aPattern));
  nsCAutoString patt(aPattern);
  ToLowerCase(patt);
  pattern = strdup(patt.get());
  NS_ASSERTION(pattern, "failed to copy pattern");
  if (!pattern)
    goto cleanup_and_return;

  rslt = gFT2FontCatalog->ParseXLFD(pattern, &foundry, &family,
                                    &charset, &encoding);
  if (!rslt)
    goto cleanup_and_return;

  // unable to handle "name-charset-*"
  if (charset && !encoding) {
    goto cleanup_and_return;
  }
  
  aFC = gFT2FontCatalog->NewFontCatalog();
  nsFT2FontCatalog::GetFontNames(aPattern, aFC);
  
  for (i=0; i<aFC->numFonts; i++) {
    nsFontCatalogEntry *fce = aFC->fonts[i];
    if (!charset) { // get all encoding
      FONT_CATALOG_PRINTF(("found FreeType %s-%s-*-*", fce->mFoundryName,
                           fce->mFamilyName));
      for (j=0; j<32; j++) {
        unsigned long bit = 1 << j;
        if (bit & fce->mCodePageRange1) {
          charSetName = gFT2FontCatalog->GetRange1CharSetName(bit);
          NS_ASSERTION(charSetName, "failed to get charset name");
          if (!charSetName)
            continue;
          node = LoadNode(fce, charSetName, aNodes);
        }
        if (bit & fce->mCodePageRange2) {
          charSetName = gFT2FontCatalog->GetRange2CharSetName(bit);
          if (!charSetName)
            continue;
          LoadNode(fce, charSetName, aNodes);
        }
      }
      if (!foundry && family && fce->mFlags&FCE_FLAGS_SYMBOL) {
        // the "registry-encoding" is not used but LoadNode will fail without
        // some value for this
        LoadNode(fce, "symbol-fontspecific", aNodes);
      }
    }

    if (charset && encoding) { // get this specific encoding
      PRUint32 cpr1_bits, cpr2_bits;
      nsCAutoString charsetName(charset);
      charsetName.Append('-');
      charsetName.Append(encoding);
      CharSetNameToCodeRangeBits(charsetName.get(), &cpr1_bits, &cpr2_bits);
      if (!(cpr1_bits & fce->mCodePageRange1)
          && !(cpr2_bits & fce->mCodePageRange2))
        continue;
      FONT_CATALOG_PRINTF(("found FreeType -%s-%s-%s",
                           fce->mFamilyName,charset,encoding));
      LoadNode(fce, charsetName.get(), aNodes);
    }
  }

  FREE_IF(pattern);
  return;

cleanup_and_return:
  FONT_CATALOG_PRINTF(("nsFT2FontNode::GetFontNames failed"));
  FREE_IF(pattern);
  return;
}

nsFontNode*
nsFT2FontNode::LoadNode(nsFontCatalogEntry *aFce, const char *aCharSetName,
                        nsFontNodeArray* aNodes)
{
  if (!gFT2FontCatalog)
    return nsnull;

  nsFontCharSetMap *charSetMap = GetCharSetMap(aCharSetName);
  if (!charSetMap->mInfo) {
    return nsnull;
  }
  const char *foundry;
  foundry = gFT2FontCatalog->GetFoundry(aFce);
  nsCAutoString nodeName(foundry);
  nodeName.Append('-');
  nodeName.Append(aFce->mFamilyName);
  nodeName.Append('-');
  nodeName.Append(aCharSetName);
  nsCStringKey key(nodeName);
  nsFontNode* node = (nsFontNode*) mFreeTypeNodes->Get(&key);
  if (!node) {
    node = new nsFontNode;
    if (!node) {
      return nsnull;
    }
    mFreeTypeNodes->Put(&key, node);
    node->mName = nodeName;
    nsFontCharSetMap *charSetMap = GetCharSetMap(aCharSetName);
    node->mCharSetInfo = charSetMap->mInfo;
  }

  int styleIndex;
  if (aFce->mStyleFlags & FT_STYLE_FLAG_ITALIC)
    styleIndex = NS_FONT_STYLE_ITALIC;
  else
    styleIndex = NS_FONT_STYLE_NORMAL;
  nsFontStyle* style = node->mStyles[styleIndex];
  if (!style) {
    style = new nsFontStyle;
    if (!style) {
      return nsnull;
    }
    node->mStyles[styleIndex] = style;
  }

  int weightIndex = WEIGHT_INDEX(aFce->mWeight);
  nsFontWeight* weight = style->mWeights[weightIndex];
  if (!weight) {
    weight = new nsFontWeight;
    if (!weight) {
      return nsnull;
    }
    style->mWeights[weightIndex] = weight;
  }

  nsFontStretch* stretch = weight->mStretches[aFce->mWidth];
  if (!stretch) {
    stretch = new nsFontStretch;
    if (!stretch) {
      return nsnull;
    }
    weight->mStretches[aFce->mWidth] = stretch;
  }
  if (!stretch->mFreeTypeFaceID) {
    stretch->mFreeTypeFaceID = nsFreeTypeGetFaceID(aFce);
  }
  if (aNodes) {
    int i, n, found = 0;
    n = aNodes->Count();
    for (i=0; i<n; i++) {
      if (aNodes->GetElement(i) == node) {
        found = 1;
      }
    }
    if (!found) {
      aNodes->AppendElement(node);
    }
  }
  return node;
}

PRBool
nsFT2FontNode::LoadNodeTable(nsFontCatalog *aFontCatalog)
{
  if (!gFT2FontCatalog)
    return PR_FALSE;

  int i, j;
 
  for (i=0; i<aFontCatalog->numFonts; i++) {
    const char *charsetName;
    nsFontCatalogEntry *fce = aFontCatalog->fonts[i];
    if ((!fce->mFlags&FCE_FLAGS_ISVALID)
        || (fce->mWeight < 100) || (fce->mWeight > 900) || (fce->mWidth > 8))
      continue;
    for (j=0; j<32; j++) {
      unsigned long bit = 1 << j;
      if (!(bit & fce->mCodePageRange1))
        continue;
      charsetName = gFT2FontCatalog->GetRange1CharSetName(bit);
      NS_ASSERTION(charsetName, "failed to get charset name");
      if (!charsetName)
        continue;
      LoadNode(fce, charsetName, nsnull);
    }
    for (j=0; j<32; j++) {
      unsigned long bit = 1 << j;
      if (!(bit & fce->mCodePageRange2))
        continue;
      charsetName = gFT2FontCatalog->GetRange2CharSetName(bit);
      if (!charsetName)
        continue;
      LoadNode(fce, charsetName, nsnull);
    }
  }

  return 0;
}

#endif

