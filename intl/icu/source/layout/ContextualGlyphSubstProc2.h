/*
 *
 * (C) Copyright IBM Corp.  and others 1998-2014 - All Rights Reserved
 *
 */

#ifndef __CONTEXTUALGLYPHSUBSTITUTIONPROCESSOR2_H
#define __CONTEXTUALGLYPHSUBSTITUTIONPROCESSOR2_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor2.h"
#include "StateTableProcessor2.h"
#include "ContextualGlyphSubstitution.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

class ContextualGlyphSubstitutionProcessor2 : public StateTableProcessor2
{
public:
    virtual void beginStateTable();

    virtual le_uint16 processStateEntry(LEGlyphStorage &glyphStorage, le_int32 &currGlyph, EntryTableIndex2 index, LEErrorCode &success);

    virtual void endStateTable();

    ContextualGlyphSubstitutionProcessor2(const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success);
    virtual ~ContextualGlyphSubstitutionProcessor2();

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
    ContextualGlyphSubstitutionProcessor2();
    TTGlyphID lookup(le_uint32 offset, LEGlyphID gid, LEErrorCode &success);

protected:
    LEReferenceToArrayOf<le_uint32>           perGlyphTable;
    LEReferenceToArrayOf<ContextualGlyphStateEntry2> entryTable;

    le_int16 perGlyphTableFormat;
    le_int32 markGlyph;

    LEReferenceTo<ContextualGlyphHeader2> contextualGlyphHeader;

};

U_NAMESPACE_END
#endif
