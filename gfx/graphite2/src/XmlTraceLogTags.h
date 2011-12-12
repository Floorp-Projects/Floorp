/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street, 
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#pragma once
#include "Main.h"
#include <graphite2/Types.h>
#include <graphite2/XmlLog.h>

namespace graphite2 {

#ifndef DISABLE_TRACING

// start this at same line number as in XmlTraceLogTags.cpp
enum XmlTraceLogElement {
    ElementTopLevel,
    ElementFace,
    ElementGlyphs,
    ElementGlyphFace,
    ElementAttr,
    ElementSilf,
    ElementSilfSub,
    ElementPass,
    ElementPseudo,
    ElementClassMap,
    ElementLookupClass,
    ElementLookup,
    ElementRange,
    ElementRuleMap,
    ElementRule,
    ElementStartState,
    ElementStateTransitions,
    ElementRow,
    ElementData,
    ElementConstraint,
    ElementConstraints,
    ElementActions,
    ElementAction,
    ElementFeatures,
    ElementFeature,
    ElementFeatureSetting,
    ElementSegment,
    ElementSlot,
    ElementText,
    ElementOpCode,
    ElementTestRule,
    ElementDoRule,
    ElementRunPass,
    ElementParams,
    ElementPush,
    ElementSubSeg,
    ElementSegCache,
    ElementSegCacheEntry,
    ElementGlyph,
    ElementPassResult,

    ElementError,
    ElementWarning,
    NumElements // Last
};



// start this at same line number as in XmlTraceLogTags.cpp
enum XmlTraceLogAttribute {
    AttrIndex,
    AttrVersion,
    AttrMajor,
    AttrMinor,
    AttrNum,
    AttrGlyphId,
    AttrAdvance,
    AttrAdvanceX,
    AttrAdvanceY,
    AttrAttrId,
    AttrAttrVal,
    AttrCompilerMajor,
    AttrCompilerMinor,
    AttrNumPasses,
    AttrSubPass,
    AttrPosPass,
    AttrJustPass,
    AttrBidiPass,
    AttrPreContext,
    AttrPostContext,
    AttrPseudoGlyph,
    AttrBreakWeight,
    AttrDirectionality,
    AttrNumJustLevels,
    AttrLigComp,
    AttrUserDefn,
    AttrNumLigComp,
    AttrNumCritFeatures,
    AttrNumScripts,
    AttrLBGlyph,
    AttrNumPseudo,
    AttrNumClasses,
    AttrNumLinear,
    AttrPassId,
    AttrFlags,
    AttrMaxRuleLoop,
    AttrMaxRuleContext,
    AttrMaxBackup,
    AttrNumRules,
    AttrNumRows,
    AttrNumTransition,
    AttrNumSuccess,
    AttrNumColumns,
    AttrNumRanges,
    AttrMinPrecontext,
    AttrMaxPrecontext,
    AttrFirstId,
    AttrLastId,
    AttrColId,
    AttrSuccessId,
    AttrRuleId,
    AttrContextLen,
    AttrState,
    AttrValue,
    AttrSortKey,
    AttrPrecontext,
    AttrAction,
    AttrActionCode,
    Attr0,
    Attr1,
    Attr2,
    Attr3,
    Attr4,
    Attr5,
    Attr6,
    Attr7,
    AttrLabel,
    AttrLength,
    AttrX,
    AttrY,
    AttrBefore,
    AttrAfter,
    AttrEncoding,
    AttrName,
    AttrResult,
    AttrDefault,
    AttrAccessCount,
    AttrLastAccess,
    AttrMisses,

    NumAttributes // Last
};

struct XmlTraceLogTag
{
public:
    XmlTraceLogTag(const char * name, uint32 flags) : mName(name), mFlags(flags) {}
    const char * mName;
    uint32 mFlags;
};

extern const XmlTraceLogTag xmlTraceLogElements[NumElements];
extern const char * xmlTraceLogAttributes[NumAttributes];

#endif // !DISABLE_TRACING

} // namespace graphite2
