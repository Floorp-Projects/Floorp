/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*- */
/* 
 * abprprty.h
 * John Sun
 * 2/18/98 10:29:50 AM
 */

#include <unistring.h>
#include "ptrarray.h"
#include "xp_mcom.h"

#ifndef __ABSTRACTPROPERTY_H_
#define __ABSTRACTPROPERTY_H_

/* pure interface */
class AbstractProperty
{
    virtual void * getValue() const { PR_ASSERT(FALSE); return 0;}
    virtual void setValue(void * value) { if (value != 0) {} PR_ASSERT(FALSE); }
   
    virtual ICalProperty * clone() { PR_ASSERT(FALSE); return 0; }
    virtual t_bool isValid() { PR_ASSERT(FALSE); return FALSE; }
 
    /* return human readable string */
    virtual UnicodeString & toString(UnicodeString & out) { PR_ASSERT(FALSE); return out; }
    virtual UnicodeString & toString(UnicodeString & dateFmt,
        UnicodeString & out) { PR_ASSERT(FALSE); if (dateFmt.size() > 0) {} return out; }
    /* ical format of value */
    virtual UnicodeString & toExportString(UnicodeString & out) { PR_ASSERT(FALSE); return out; }
};

#endif /* __ABSTRACTPROPERTY_H_ */

