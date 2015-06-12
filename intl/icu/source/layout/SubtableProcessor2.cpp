/*
 *
 * (C) Copyright IBM Corp.  and others 1998-2014 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor2.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

SubtableProcessor2::SubtableProcessor2()
{
}

SubtableProcessor2::SubtableProcessor2(const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success)
  : length(0), coverage(0), subtableFeatures(0L), subtableHeader(morphSubtableHeader, success)
{
  if(LE_FAILURE(success)) return;

  length = SWAPL(subtableHeader->length);
  coverage = SWAPL(subtableHeader->coverage);
  subtableFeatures = SWAPL(subtableHeader->subtableFeatures);
}

SubtableProcessor2::~SubtableProcessor2()
{
}

U_NAMESPACE_END
