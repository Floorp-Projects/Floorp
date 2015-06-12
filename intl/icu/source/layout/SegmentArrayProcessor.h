/*
 *
 * (C) Copyright IBM Corp. 1998-2014 - All Rights Reserved
 *
 */

#ifndef __SEGMENTARRAYPROCESSOR_H
#define __SEGMENTARRAYPROCESSOR_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "NonContextualGlyphSubst.h"
#include "NonContextualGlyphSubstProc.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

class SegmentArrayProcessor : public NonContextualGlyphSubstitutionProcessor
{
public:
    virtual void process(LEGlyphStorage &glyphStorage, LEErrorCode &success);

    SegmentArrayProcessor(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success);

    virtual ~SegmentArrayProcessor();

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
    SegmentArrayProcessor();

protected:
    LEReferenceTo<SegmentArrayLookupTable> segmentArrayLookupTable;

};

U_NAMESPACE_END
#endif

