/*
 *
 * (C) Copyright IBM Corp.  and others 1998-2014 - All Rights Reserved
 *
 */

#ifndef __SINGLETABLEPROCESSOR2_H
#define __SINGLETABLEPROCESSOR2_H

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

class SingleTableProcessor2 : public NonContextualGlyphSubstitutionProcessor2
{
public:
    virtual void process(LEGlyphStorage &glyphStorage, LEErrorCode &success);

    SingleTableProcessor2(const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success);

    virtual ~SingleTableProcessor2();

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
    SingleTableProcessor2();

protected:
    LEReferenceTo<SingleTableLookupTable> singleTableLookupTable;

};

U_NAMESPACE_END
#endif
