/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* shareddateformat.h
*/

#ifndef __SHARED_DATETIMEPATTERNGENERATOR_H__
#define __SHARED_DATETIMEPATTERNGENERATOR_H__

#include "unicode/utypes.h"
#include "sharedobject.h"

U_NAMESPACE_BEGIN

class DateTimePatternGenerator;

class U_I18N_API SharedDateTimePatternGenerator : public SharedObject {
public:
    SharedDateTimePatternGenerator(DateTimePatternGenerator *dtpgToAdopt) : ptr(dtpgToAdopt) { }
    virtual ~SharedDateTimePatternGenerator();
    const DateTimePatternGenerator *get() const { return ptr; }
    const DateTimePatternGenerator *operator->() const { return ptr; }
    const DateTimePatternGenerator &operator*() const { return *ptr; }
private:
    DateTimePatternGenerator *ptr;
    SharedDateTimePatternGenerator(const SharedDateTimePatternGenerator &);
    SharedDateTimePatternGenerator &operator=(const SharedDateTimePatternGenerator &);
};

U_NAMESPACE_END

#endif
