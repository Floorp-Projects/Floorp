/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM iMThreadContext.idl
 */

#ifndef __gen_iMThreadContext_h__
#define __gen_iMThreadContext_h__

#include "nsISupports.h"
#include "nsIComponentManager.h"


/* starting interface:    iMThreadContext */
#define IMTHREADCONTEXT_IID_STR "a7b26685-9816-4eb6-a075-36f539a7a823"

#define IMTHREADCONTEXT_IID \
  {0xa7b26685, 0x9816, 0x4eb6, \
    { 0xa0, 0x75, 0x36, 0xf5, 0x39, 0xa7, 0xa8, 0x23 }}

class NS_NO_VTABLE iMThreadContext : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(IMTHREADCONTEXT_IID)

  /* string GetNext (); */
  NS_IMETHOD GetNext(char **_retval) = 0;

  /* nsIComponentManager GetComponentManager (); */
  NS_IMETHOD GetComponentManager(nsIComponentManager **_retval) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_IMTHREADCONTEXT \
  NS_IMETHOD GetNext(char **_retval); \
  NS_IMETHOD GetComponentManager(nsIComponentManager **_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_IMTHREADCONTEXT(_to) \
  NS_IMETHOD GetNext(char **_retval) { return _to ## GetNext(_retval); } \
  NS_IMETHOD GetComponentManager(nsIComponentManager **_retval) { return _to ## GetComponentManager(_retval); } 

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

/* string GetNext (); */
NS_IMETHODIMP _MYCLASS_::GetNext(char **_retval)
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
#define MTHREADCONTEXT_PROGID "component://netscape/blackwood/blackconnect/test/mthreads/MThreadContext"

#endif /* __gen_iMThreadContext_h__ */
