/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)cxx_int.h	10.5 (Sleepycat) 4/10/98
 */

#ifndef _CXX_INT_H_
#define _CXX_INT_H_

// private data structures known to the implementation only

#include <assert.h>             // used by defines below

//
// Using FooImp classes will allow the implementation to change in the
// future without any modification to user code or even to header files
// that the user includes. FooImp * is just like void * except that it
// provides a little extra protection, since you cannot randomly assign
// any old pointer to a FooImp* as you can with void *.  Currently, a
// pointer to such an opaque class is always just a pointer to the
// appropriate underlying implementation struct.  These are converted
// back and forth using the various overloaded wrap()/unwrap() methods.
// This is essentially a use of the "Bridge" Design Pattern.
//
// WRAPPED_CLASS implements the appropriate wrap() and unwrap() methods
// for a wrapper class that has an underlying pointer representation.
//
#define WRAPPED_CLASS(_WRAPPER_CLASS, _IMP_CLASS, _WRAPPED_TYPE)           \
                                                                           \
        class _IMP_CLASS {};                                               \
                                                                           \
        inline _WRAPPED_TYPE unwrap(_WRAPPER_CLASS *val)                   \
        {                                                                  \
            if (!val) return 0;                                            \
            return (_WRAPPED_TYPE)(val->imp());                            \
        }                                                                  \
                                                                           \
        inline const _WRAPPED_TYPE unwrapConst(const _WRAPPER_CLASS *val)  \
        {                                                                  \
            if (!val) return 0;                                            \
            return (const _WRAPPED_TYPE)(val->imp());                      \
        }                                                                  \
                                                                           \
        inline _IMP_CLASS *wrap(_WRAPPED_TYPE val)                         \
        {                                                                  \
            return (_IMP_CLASS*)val;                                       \
        }

WRAPPED_CLASS(DbLockTab, DbLockTabImp, DB_LOCKTAB*)
WRAPPED_CLASS(DbLog, DbLogImp, DB_LOG*)
WRAPPED_CLASS(DbMpool, DbMpoolImp, DB_MPOOL*)
WRAPPED_CLASS(DbMpoolFile, DbMpoolFileImp, DB_MPOOLFILE*)
WRAPPED_CLASS(Db, DbImp, DB*)
WRAPPED_CLASS(DbTxn, DbTxnImp, DB_TXN*)
WRAPPED_CLASS(DbTxnMgr, DbTxnMgrImp, DB_TXNMGR*)

// Macros that handle detected errors, in case we want to
// change the default behavior.  runtime_error() throws an
// exception by default.
//
// Since it's unusual to throw an exception in a destructor,
// we have a separate macro.  For now, we silently ignore such
// detected errors.
//
#define DB_ERROR(caller, ecode) \
    DbEnv::runtime_error(caller, ecode)

#define DB_DESTRUCTOR_ERROR(caller, ecode) \
    DbEnv::runtime_error(caller, ecode, 1)


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// These defines are for tedious flag or field set/get access methods.
//

// Define setName() and getName() methods that twiddle
// the _flags field.
//
#define DB_FLAG_METHODS(_class, _flags, _cxx_name, _flag_name) \
                                                               \
void _class::set##_cxx_name(int onOrOff)                       \
{                                                              \
    if (onOrOff)                                               \
        _flags |= _flag_name;                                  \
    else                                                       \
        _flags &= ~(_flag_name);                               \
}                                                              \
                                                               \
int _class::get##_cxx_name() const                             \
{                                                              \
    return (_flags & _flag_name) ? 1 : 0;                      \
}


#define DB_RO_ACCESS(_class, _type, _cxx_name, _field)         \
                                                               \
_type _class::get_##_cxx_name() const                          \
{                                                              \
    return _field;                                             \
}

#define DB_WO_ACCESS(_class, _type, _cxx_name, _field)         \
                                                               \
void _class::set_##_cxx_name(_type value)                      \
{                                                              \
    _field = value;                                            \
}                                                              \

#define DB_RW_ACCESS(_class, _type, _cxx_name, _field)         \
        DB_RO_ACCESS(_class, _type, _cxx_name, _field)         \
        DB_WO_ACCESS(_class, _type, _cxx_name, _field)

#endif /* !_CXX_INT_H_ */
