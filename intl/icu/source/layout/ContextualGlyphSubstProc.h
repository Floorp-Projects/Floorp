/*
 *
 * (C) Copyright IBM Corp. 1998-2014 - All Rights Reserved
 *
 */

#ifndef __CONTEXTUALGLYPHSUBSTITUTIONPROCESSOR_H
#define __CONTEXTUALGLYPHSUBSTITUTIONPROCESSOR_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "StateTableProcessor.h"
#include "ContextualGlyphSubstitution.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

class ContextualGlyphSubstitutionProcessor : public StateTableProcessor
{
public:
    virtual void beginStateTable();

    virtual ByteOffset processStateEntry(LEGlyphStorage &glyphStorage, le_int32 &currGlyph, EntryTableIndex index);

    virtual void endStateTable();

    ContextualGlyphSubstitutionProcessor(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success);
    virtual ~ContextualGlyphSubstitutionProcessor();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @deprecated ICU 54. See {@link icu::LayoutEngine}
     */
    static UClassID getStaticClassID();

private:
    ContextualGlyphSubstitutionProcessor();

protected:
    ByteOffset substitutionTableOffset;
    LEReferenceToArrayOf<ContextualGlyphSubstitutionStateEntry> entryTable;
    LEReferenceToArrayOf<le_int16> int16Table;
    le_int32 markGlyph;

    LEReferenceTo<ContextualGlyphSubstitutionHeader> contextualGlyphSubstitutionHeader;

};

U_NAMESPACE_END
#endif
