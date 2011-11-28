/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *   Jonathan Kew <jfkthame@gmail.com>
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

#ifndef GFX_TEXT_RUN_WORD_CACHE_H
#define GFX_TEXT_RUN_WORD_CACHE_H

#include "gfxFont.h"

/**
 * Cache individual "words" (strings delimited by white-space or white-space-like
 * characters that don't involve kerning or ligatures) in textruns.
  */
class THEBES_API gfxTextRunWordCache {
public:
    enum {
      TEXT_IN_CACHE = 0x10000000,

      /**
       * nsTextFrameThebes sets these, but they're defined here rather than in
       * nsTextFrameUtils.h because the cache also needs to check the _INCOMING flag
       */
      TEXT_TRAILING_ARABICCHAR = 0x20000000,
      /**
       * When set, the previous character for this textrun was an Arabic
       * character.  This is used for the context detection necessary for
       * bidi.numeral implementation.
       */
      TEXT_INCOMING_ARABICCHAR = 0x40000000,

      TEXT_UNUSED_FLAGS = 0x80000000
    };

    /**
     * Create a textrun using cached words.
     * Invalid characters (see gfxFontGroup::IsInvalidChar) will be automatically
     * treated as invisible missing.
     * @param aFlags the flag TEXT_IS_ASCII must be set by the caller,
     * if applicable; TEXT_IN_CACHE is added if we have a reference to
     * the textrun in the cache and RemoveTextRun must be called when the
     * textrun dies.
     */
    static gfxTextRun *MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                                   gfxFontGroup *aFontGroup,
                                   const gfxFontGroup::Parameters *aParams,
                                   PRUint32 aFlags);
    /**
     * Create a textrun using cached words.
     * Invalid characters (see gfxFontGroup::IsInvalidChar) will be automatically
     * treated as invisible missing.
     * @param aFlags the flag TEXT_IS_ASCII must be set by the caller,
     * if applicable; TEXT_IN_CACHE is added if we have a reference to
     * the textrun in the cache and RemoveTextRun must be called when the
     * textrun dies.
     */
    static gfxTextRun *MakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                                   gfxFontGroup *aFontGroup,
                                   const gfxFontGroup::Parameters *aParams,
                                   PRUint32 aFlags);

    /**
     * Remove a textrun from the cache. This must be called before aTextRun
     * is deleted! The text in the textrun must still be valid.
     */
    static void RemoveTextRun(gfxTextRun *aTextRun);

    /**
     * Flush the textrun cache. This must be called if a configuration
     * change that would affect textruns is applied.
     */
    static void Flush();

    /**
     * This adds the storage used for each textRun to the total, and sets the
     * TEXT_RUN_MEMORY_ACCOUNTED flag to avoid double- accounting. (Runs with
     * this flag already set will be skipped.)
     */
    static size_t MaybeSizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf);

    /**
     * This clears the TEXT_RUN_MEMORY_ACCOUNTED flag on each textRun found.
     */
    static void ResetSizeOfAccountingFlags();

protected:
    friend class gfxPlatform;

    static nsresult Init();
    static void Shutdown();
};

#endif /* GFX_TEXT_RUN_WORD_CACHE_H */
