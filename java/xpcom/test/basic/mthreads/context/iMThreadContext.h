/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM iMThreadContext.idl
 */

#ifndef __gen_iMThreadContext_h__
#define __gen_iMThreadContext_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

#ifndef __gen_nsIComponentManager_h__
#include "nsIComponentManager.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    iMThreadContext */
#define IMTHREADCONTEXT_IID_STR "a7b26685-9816-4eb6-a075-36f539a7a823"

#define IMTHREADCONTEXT_IID \
  {0xa7b26685, 0x9816, 0x4eb6, \
    { 0xa0, 0x75, 0x36, 0xf5, 0x39, 0xa7, 0xa8, 0x23 }}

class NS_NO_VTABLE iMThreadContext : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(IMTHREADCONTEXT_IID)

  /* long GetStages (); */
  NS_IMETHOD GetStages(PRInt32 *_retval) = 0;

  /* long GetThreads (); */
  NS_IMETHOD GetThreads(PRInt32 *_retval) = 0;

  /* string GetResFile (); */
  NS_IMETHOD GetResFile(char **_retval) = 0;

  /* string GetPath (in boolean i); */
  NS_IMETHOD GetPath(PRBool i, char **_retval) = 0;

  /* string GetContractID (in long i, in long j); */
  NS_IMETHOD GetContractID(PRInt32 i, PRInt32 j, char **_retval) = 0;

  /* nsIComponentManager GetComponentManager (); */
  NS_IMETHOD GetComponentManager(nsIComponentManager **_retval) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_IMTHREADCONTEXT \
  NS_IMETHOD GetStages(PRInt32 *_retval); \
  NS_IMETHOD GetThreads(PRInt32 *_retval); \
  NS_IMETHOD GetResFile(char **_retval); \
  NS_IMETHOD GetPath(PRBool i, char **_retval); \
  NS_IMETHOD GetContractID(PRInt32 i, PRInt32 j, char **_retval); \
  NS_IMETHOD GetComponentManager(nsIComponentManager **_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_IMTHREADCONTEXT(_to) \
  NS_IMETHOD GetStages(PRInt32 *_retval) { return _to ## GetStages(_retval); } \
  NS_IMETHOD GetThreads(PRInt32 *_retval) { return _to ## GetThreads(_retval); } \
  NS_IMETHOD GetResFile(char **_retval) { return _to ## GetResFile(_retval); } \
  NS_IMETHOD GetPath(PRBool i, char **_retval) { return _to ## GetPath(i, _retval); } \
  NS_IMETHOD GetContractID(PRInt32 i, PRInt32 j, char **_retval) { return _to ## GetContractID(i, j, _retval); } \
  NS_IMETHOD GetComponentManager(nsIComponentManager **_retval) { return _to ## GetComponentManager(_retval); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_IMTHREADCONTEXT(_to) \
  NS_IMETHOD GetStages(PRInt32 *_retval) { return !_to ## ? NS_ERROR_NULL_POINTER : _to ##-> GetStages(_retval); } \
  NS_IMETHOD GetThreads(PRInt32 *_retval) { return !_to ## ? NS_ERROR_NULL_POINTER : _to ##-> GetThreads(_retval); } \
  NS_IMETHOD GetResFile(char **_retval) { return !_to ## ? NS_ERROR_NULL_POINTER : _to ##-> GetResFile(_retval); } \
  NS_IMETHOD GetPath(PRBool i, char **_retval) { return !_to ## ? NS_ERROR_NULL_POINTER : _to ##-> GetPath(i, _retval); } \
  NS_IMETHOD GetContractID(PRInt32 i, PRInt32 j, char **_retval) { return !_to ## ? NS_ERROR_NULL_POINTER : _to ##-> GetContractID(i, j, _retval); } \
  NS_IMETHOD GetComponentManager(nsIComponentManager **_retval) { return !_to ## ? NS_ERROR_NULL_POINTER : _to ##-> GetComponentManager(_retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public iMThreadContext
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMTHREADCONTEXT

  _MYCLASS_();
  virtual ~_MYCLASS_();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, iMThreadContext)

_MYCLASS_::_MYCLASS_()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* long GetStages (); */
NS_IMETHODIMP _MYCLASS_::GetStages(PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* long GetThreads (); */
NS_IMETHODIMP _MYCLASS_::GetThreads(PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string GetResFile (); */
NS_IMETHODIMP _MYCLASS_::GetResFile(char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string GetPath (in boolean i); */
NS_IMETHODIMP _MYCLASS_::GetPath(PRBool i, char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string GetContractID (in long i, in long j); */
NS_IMETHODIMP _MYCLASS_::GetContractID(PRInt32 i, PRInt32 j, char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIComponentManager GetComponentManager (); */
NS_IMETHODIMP _MYCLASS_::GetComponentManager(nsIComponentManager **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif

#define MTHREADCONTEXT_CID                 \
{ /* 139b8350-280a-46d0-8afc-f1939173c2ea */         \
     0x139b8350,                                     \
     0x280a,                                         \
     0x46d0,                                         \
    {0x8a, 0xfc, 0xf1, 0x93, 0x91, 0x73, 0xc2, 0xea} \
}
#define MTHREADCONTEXT_PROGID "@mozilla/blackwood/blackconnect/test/params/MThreadContext;1"

#endif /* __gen_iMThreadContext_h__ */
