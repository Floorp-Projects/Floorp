/*
 *
 * (C) Copyright IBM Corp.  and others 1998-2014 - All Rights Reserved
 *
 */

#ifndef __SIMPLEARRAYPROCESSOR2_H
#define __SIMPLEARRAYPROCESSOR2_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor2.h"
#include "NonContextualGlyphSubst.h"
#include "NonContextualGlyphSubstProc2.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

class SimpleArrayProcessor2 : public NonContextualGlyphSubstitutionProcessor2
{
public:
    virtual void process(LEGlyphStorage &glyphStorage, LEErrorCode &success);

    SimpleArrayProcessor2(const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success);

    virtual ~SimpleArrayProcessor2();

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
    SimpleArrayProcessor2();

protected:
    LEReferenceTo<SimpleArrayLookupTable> simpleArrayLookupTable;
    LEReferenceToArrayOf<LookupValue> valueArray;

};

U_NAMESPACE_END
#endif

