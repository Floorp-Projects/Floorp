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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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

#ifndef GFX_DWRITETEXTANALYSIS_H
#define GFX_DWRITETEXTANALYSIS_H

#include "gfxDWriteCommon.h"

// Helper source/sink class for text analysis.
class TextAnalysis
    :   public IDWriteTextAnalysisSource,
        public IDWriteTextAnalysisSink        
{
public:

    // IUnknown interface
    IFACEMETHOD(QueryInterface)(IID const& iid, OUT void** ppObject)
    {
        if (iid == __uuidof(IDWriteTextAnalysisSource)) {
            *ppObject = static_cast<IDWriteTextAnalysisSource*>(this);
            return S_OK;
        } else if (iid == __uuidof(IDWriteTextAnalysisSink)) {
            *ppObject = static_cast<IDWriteTextAnalysisSink*>(this);
            return S_OK;
        } else if (iid == __uuidof(IUnknown)) {
            *ppObject = 
                static_cast<IUnknown*>(static_cast<IDWriteTextAnalysisSource*>(this));
            return S_OK;
        } else {
            return E_NOINTERFACE;
        }
    }

    IFACEMETHOD_(ULONG, AddRef)()
    {
        return 1;
    }

    IFACEMETHOD_(ULONG, Release)()
    {
        return 1;
    }

    // A single contiguous run of characters containing the same analysis 
    // results.
    struct Run
    {
        UINT32 mTextStart;   // starting text position of this run
        UINT32 mTextLength;  // number of contiguous code units covered
        UINT32 mGlyphStart;  // starting glyph in the glyphs array
        UINT32 mGlyphCount;  // number of glyphs associated with this run of 
                             // text
        DWRITE_SCRIPT_ANALYSIS mScript;
        UINT8 mBidiLevel;
        bool mIsSideways;

        inline bool ContainsTextPosition(UINT32 aTextPosition) const
        {
            return aTextPosition >= mTextStart
                && aTextPosition <  mTextStart + mTextLength;
        }

        Run *nextRun;
    };

public:
    TextAnalysis(const wchar_t* text,
                 UINT32 textLength,
                 const wchar_t* localeName,
                 DWRITE_READING_DIRECTION readingDirection);

    ~TextAnalysis();

    STDMETHODIMP GenerateResults(IDWriteTextAnalyzer* textAnalyzer,
                                 Run **runHead);

    // IDWriteTextAnalysisSource implementation

    IFACEMETHODIMP GetTextAtPosition(UINT32 textPosition,
                                     OUT WCHAR const** textString,
                                     OUT UINT32* textLength);

    IFACEMETHODIMP GetTextBeforePosition(UINT32 textPosition,
                                         OUT WCHAR const** textString,
                                         OUT UINT32* textLength);

    IFACEMETHODIMP_(DWRITE_READING_DIRECTION) 
        GetParagraphReadingDirection() throw();

    IFACEMETHODIMP GetLocaleName(UINT32 textPosition,
                                 OUT UINT32* textLength,
                                 OUT WCHAR const** localeName);

    IFACEMETHODIMP 
        GetNumberSubstitution(UINT32 textPosition,
                              OUT UINT32* textLength,
                              OUT IDWriteNumberSubstitution** numberSubstitution);

    // IDWriteTextAnalysisSink implementation

    IFACEMETHODIMP 
        SetScriptAnalysis(UINT32 textPosition,
                          UINT32 textLength,
                          DWRITE_SCRIPT_ANALYSIS const* scriptAnalysis);

    IFACEMETHODIMP 
        SetLineBreakpoints(UINT32 textPosition,
                           UINT32 textLength,
                           const DWRITE_LINE_BREAKPOINT* lineBreakpoints);

    IFACEMETHODIMP SetBidiLevel(UINT32 textPosition,
                                UINT32 textLength,
                                UINT8 explicitLevel,
                                UINT8 resolvedLevel);

    IFACEMETHODIMP 
        SetNumberSubstitution(UINT32 textPosition,
                              UINT32 textLength,
                              IDWriteNumberSubstitution* numberSubstitution);

protected:
    Run *FetchNextRun(IN OUT UINT32* textLength);

    void SetCurrentRun(UINT32 textPosition);

    void SplitCurrentRun(UINT32 splitPosition);

protected:
    // Input
    // (weak references are fine here, since this class is a transient
    //  stack-based helper that doesn't need to copy data)
    UINT32 mTextLength;
    const wchar_t* mText;
    const wchar_t* mLocaleName;
    DWRITE_READING_DIRECTION mReadingDirection;

    // Current processing state.
    Run *mCurrentRun;

    // Output is a list of runs starting here
    Run  mRunHead;
};

#endif /* GFX_DWRITETEXTANALYSIS_H */
