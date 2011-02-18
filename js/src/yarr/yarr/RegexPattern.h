/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef RegexPattern_h
#define RegexPattern_h

#include "jsvector.h"
#include "yarr/jswtfbridge.h"
#include "yarr/yarr/RegexCommon.h"


namespace JSC { namespace Yarr {

#define RegexStackSpaceForBackTrackInfoPatternCharacter 1 // Only for !fixed quantifiers.
#define RegexStackSpaceForBackTrackInfoCharacterClass 1 // Only for !fixed quantifiers.
#define RegexStackSpaceForBackTrackInfoBackReference 2
#define RegexStackSpaceForBackTrackInfoAlternative 1 // One per alternative.
#define RegexStackSpaceForBackTrackInfoParentheticalAssertion 1
#define RegexStackSpaceForBackTrackInfoParenthesesOnce 1 // Only for !fixed quantifiers.
#define RegexStackSpaceForBackTrackInfoParenthesesTerminal 1
#define RegexStackSpaceForBackTrackInfoParentheses 4

struct PatternDisjunction;

struct CharacterRange {
    UChar begin;
    UChar end;

    CharacterRange(UChar begin, UChar end)
        : begin(begin)
        , end(end)
    {
    }
};

/*
 * Wraps a table and indicates inversion. Can be efficiently borrowed
 * between character classes, so it's refcounted.
 */
struct CharacterClassTable {
    const char* m_table;
    bool m_inverted;
    jsrefcount m_refcount;

    /* Ownership transferred to caller. */
    static CharacterClassTable *create(const char* table, bool inverted)
    {
        // FIXME: bug 574459 -- no NULL checks done by any of the callers, all
        // of which are in RegExpJitTables.h.
        /* We can't (easily) use js_new() here because the constructor is private. */
        void *memory = js_malloc(sizeof(CharacterClassTable));
        return memory ? new(memory) CharacterClassTable(table, inverted) : NULL;
    }

    void incref() { JS_ATOMIC_INCREMENT(&m_refcount); }
    void decref() { if (JS_ATOMIC_DECREMENT(&m_refcount) == 0) js_delete(this); }

private:
    CharacterClassTable(const char* table, bool inverted)
        : m_table(table)
        , m_inverted(inverted)
        , m_refcount(0)
    {
    }
};

struct CharacterClass {
    // All CharacterClass instances have to have the full set of matches and ranges,
    // they may have an optional table for faster lookups (which must match the
    // specified matches and ranges)
    CharacterClass(CharacterClassTable *table)
        : m_table(table)
    {
        if (m_table)
            m_table->incref();
    }
    ~CharacterClass()
    {
        if (m_table)
            m_table->decref();
    }
    typedef js::Vector<UChar, 0, js::SystemAllocPolicy> UChars;
    typedef js::Vector<CharacterRange, 0, js::SystemAllocPolicy> CharacterRanges;
    UChars m_matches;
    CharacterRanges m_ranges;
    UChars m_matchesUnicode;
    CharacterRanges m_rangesUnicode;
    CharacterClassTable *m_table;
};

enum QuantifierType {
    QuantifierFixedCount,
    QuantifierGreedy,
    QuantifierNonGreedy
};

struct PatternTerm {
    enum Type {
        TypeAssertionBOL,
        TypeAssertionEOL,
        TypeAssertionWordBoundary,
        TypePatternCharacter,
        TypeCharacterClass,
        TypeBackReference,
        TypeForwardReference,
        TypeParenthesesSubpattern,
        TypeParentheticalAssertion
    } type;
    bool invertOrCapture;
    union {
        UChar patternCharacter;
        CharacterClass* characterClass;
        unsigned subpatternId;
        struct {
            PatternDisjunction* disjunction;
            unsigned subpatternId;
            unsigned lastSubpatternId;
            bool isCopy;
            bool isTerminal;
        } parentheses;
    };
    QuantifierType quantityType;
    unsigned quantityCount;
    int inputPosition;
    unsigned frameLocation;

    PatternTerm(UChar ch)
        : type(PatternTerm::TypePatternCharacter)
    {
        patternCharacter = ch;
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }

    PatternTerm(CharacterClass* charClass, bool invert)
        : type(PatternTerm::TypeCharacterClass)
        , invertOrCapture(invert)
    {
        characterClass = charClass;
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }

    PatternTerm(Type type, unsigned subpatternId, PatternDisjunction* disjunction, bool invertOrCapture)
        : type(type)
        , invertOrCapture(invertOrCapture)
    {
        parentheses.disjunction = disjunction;
        parentheses.subpatternId = subpatternId;
        parentheses.isCopy = false;
        parentheses.isTerminal = false;
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }
    
    PatternTerm(Type type, bool invert = false)
        : type(type)
        , invertOrCapture(invert)
    {
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }

    PatternTerm(unsigned spatternId)
        : type(TypeBackReference)
        , invertOrCapture(false)
    {
        subpatternId = spatternId;
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }

    static PatternTerm ForwardReference()
    {
        return PatternTerm(TypeForwardReference);
    }

    static PatternTerm BOL()
    {
        return PatternTerm(TypeAssertionBOL);
    }

    static PatternTerm EOL()
    {
        return PatternTerm(TypeAssertionEOL);
    }

    static PatternTerm WordBoundary(bool invert)
    {
        return PatternTerm(TypeAssertionWordBoundary, invert);
    }
    
    bool invert()
    {
        return invertOrCapture;
    }

    bool capture()
    {
        return invertOrCapture;
    }
    
    void quantify(unsigned count, QuantifierType type)
    {
        quantityCount = count;
        quantityType = type;
    }
};

struct PatternAlternative {
    PatternAlternative(PatternDisjunction* disjunction)
        : m_parent(disjunction)
        , m_onceThrough(false)
        , m_hasFixedSize(false)
        , m_startsWithBOL(false)
        , m_containsBOL(false)
    {
    }

    PatternTerm& lastTerm()
    {
        JS_ASSERT(m_terms.length());
        return m_terms[m_terms.length() - 1];
    }
    
    void removeLastTerm()
    {
        JS_ASSERT(m_terms.length());
        m_terms.popBack();
    }
    
    void setOnceThrough()
    {
        m_onceThrough = true;
    }
    
    bool onceThrough()
    {
        return m_onceThrough;
    }

    js::Vector<PatternTerm, 0, js::SystemAllocPolicy> m_terms;
    PatternDisjunction* m_parent;
    unsigned m_minimumSize;
    bool m_onceThrough : 1;
    bool m_hasFixedSize : 1;
    bool m_startsWithBOL : 1;
    bool m_containsBOL : 1;
};

template<typename T, size_t N, class AP>
static inline void
deleteAllValues(js::Vector<T*,N,AP> &vector)
{
    for (T** t = vector.begin(); t < vector.end(); ++t)
        js_delete(*t);
}

struct PatternDisjunction {
    PatternDisjunction(PatternAlternative* parent = 0)
        : m_parent(parent)
        , m_hasFixedSize(false)
    {
    }
    
    ~PatternDisjunction()
    {
        deleteAllValues(m_alternatives);
    }

    PatternAlternative* addNewAlternative()
    {
        // FIXME: bug 574459 -- no NULL check
        PatternAlternative* alternative = js_new<PatternAlternative>(this);
        m_alternatives.append(alternative);
        return alternative;
    }

    js::Vector<PatternAlternative*, 0, js::SystemAllocPolicy> m_alternatives;
    PatternAlternative* m_parent;
    unsigned m_minimumSize;
    unsigned m_callFrameSize;
    bool m_hasFixedSize;
};

// You probably don't want to be calling these functions directly
// (please to be calling newlineCharacterClass() et al on your
// friendly neighborhood RegexPattern instance to get nicely
// cached copies).
CharacterClass* newlineCreate();
CharacterClass* digitsCreate();
CharacterClass* spacesCreate();
CharacterClass* wordcharCreate();
CharacterClass* nondigitsCreate();
CharacterClass* nonspacesCreate();
CharacterClass* nonwordcharCreate();

struct RegexPattern {
    RegexPattern(bool ignoreCase, bool multiline)
        : m_ignoreCase(ignoreCase)
        , m_multiline(multiline)
        , m_containsBackreferences(false)
        , m_containsBOL(false)
        , m_numSubpatterns(0)
        , m_maxBackReference(0)
        , newlineCached(0)
        , digitsCached(0)
        , spacesCached(0)
        , wordcharCached(0)
        , nondigitsCached(0)
        , nonspacesCached(0)
        , nonwordcharCached(0)
    {
    }

    ~RegexPattern()
    {
        deleteAllValues(m_disjunctions);
        deleteAllValues(m_userCharacterClasses);
    }

    void reset()
    {
        m_numSubpatterns = 0;
        m_maxBackReference = 0;

        m_containsBackreferences = false;
        m_containsBOL = false;

        newlineCached = 0;
        digitsCached = 0;
        spacesCached = 0;
        wordcharCached = 0;
        nondigitsCached = 0;
        nonspacesCached = 0;
        nonwordcharCached = 0;

        deleteAllValues(m_disjunctions);
        m_disjunctions.clear();
        deleteAllValues(m_userCharacterClasses);
        m_userCharacterClasses.clear();
    }

    bool containsIllegalBackReference()
    {
        return m_maxBackReference > m_numSubpatterns;
    }

    CharacterClass* newlineCharacterClass()
    {
        if (!newlineCached)
            m_userCharacterClasses.append(newlineCached = newlineCreate());
        return newlineCached;
    }
    CharacterClass* digitsCharacterClass()
    {
        if (!digitsCached)
            m_userCharacterClasses.append(digitsCached = digitsCreate());
        return digitsCached;
    }
    CharacterClass* spacesCharacterClass()
    {
        if (!spacesCached)
            m_userCharacterClasses.append(spacesCached = spacesCreate());
        return spacesCached;
    }
    CharacterClass* wordcharCharacterClass()
    {
        if (!wordcharCached)
            m_userCharacterClasses.append(wordcharCached = wordcharCreate());
        return wordcharCached;
    }
    CharacterClass* nondigitsCharacterClass()
    {
        if (!nondigitsCached)
            m_userCharacterClasses.append(nondigitsCached = nondigitsCreate());
        return nondigitsCached;
    }
    CharacterClass* nonspacesCharacterClass()
    {
        if (!nonspacesCached)
            m_userCharacterClasses.append(nonspacesCached = nonspacesCreate());
        return nonspacesCached;
    }
    CharacterClass* nonwordcharCharacterClass()
    {
        if (!nonwordcharCached)
            m_userCharacterClasses.append(nonwordcharCached = nonwordcharCreate());
        return nonwordcharCached;
    }

    typedef js::Vector<PatternDisjunction*, 4, js::SystemAllocPolicy> PatternDisjunctions;
    typedef js::Vector<CharacterClass*, 0, js::SystemAllocPolicy> CharacterClasses;
    bool m_ignoreCase : 1;
    bool m_multiline : 1;
    bool m_containsBackreferences : 1;
    bool m_containsBOL : 1;
    unsigned m_numSubpatterns;
    unsigned m_maxBackReference;
    PatternDisjunction *m_body;
    PatternDisjunctions m_disjunctions;
    CharacterClasses m_userCharacterClasses;

private:
    CharacterClass* newlineCached;
    CharacterClass* digitsCached;
    CharacterClass* spacesCached;
    CharacterClass* wordcharCached;
    CharacterClass* nondigitsCached;
    CharacterClass* nonspacesCached;
    CharacterClass* nonwordcharCached;
};

} } // namespace JSC::Yarr

#endif // RegexPattern_h
