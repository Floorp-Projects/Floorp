/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM test.idl
 */

#ifndef __gen_test_h__
#define __gen_test_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    testCallback */
#define TESTCALLBACK_IID_STR "75d2225d-0a67-4dbc-91de-78319594cce8"

#define TESTCALLBACK_IID \
  {0x75d2225d, 0x0a67, 0x4dbc, \
    { 0x91, 0xde, 0x78, 0x31, 0x95, 0x94, 0xcc, 0xe8 }}

class NS_NO_VTABLE testCallback : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(TESTCALLBACK_IID)

  /* void call (); */
  NS_IMETHOD Call(void) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_TESTCALLBACK \
  NS_IMETHOD Call(void); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_TESTCALLBACK(_to) \
  NS_IMETHOD Call(void) { return _to Call(); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_TESTCALLBACK(_to) \
  NS_IMETHOD Call(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->Call(); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public testCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_TESTCALLBACK

  _MYCLASS_();
  virtual ~_MYCLASS_();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, testCallback)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* void call (); */
NS_IMETHODIMP _MYCLASS_::Call()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


/* starting interface:    test */
#define TEST_IID_STR "1afbcf6a-e23f-4e12-b191-4c0a76cd9cec"

#define TEST_IID \
  {0x1afbcf6a, 0xe23f, 0x4e12, \
    { 0xb1, 0x91, 0x4c, 0x0a, 0x76, 0xcd, 0x9c, 0xec }}

class NS_NO_VTABLE test : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(TEST_IID)

  /* void say (in string sayIt); */
  NS_IMETHOD Say(const char *sayIt) = 0;

  /* void shout (in string shoutIt); */
  NS_IMETHOD Shout(const char *shoutIt) = 0;

  /* void poke (in string with); */
  NS_IMETHOD Poke(const char *with) = 0;

  /* PRInt32 add (in PRInt32 a, in PRInt32 b); */
  NS_IMETHOD Add(PRInt32 a, PRInt32 b, PRInt32 *_retval) = 0;

  /* string peek (); */
  NS_IMETHOD Peek(char **_retval) = 0;

  /* void callback (in testCallback cb); */
  NS_IMETHOD Callback(testCallback *cb) = 0;

  /* attribute PRInt32 intProp; */
  NS_IMETHOD GetIntProp(PRInt32 *aIntProp) = 0;
  NS_IMETHOD SetIntProp(PRInt32 aIntProp) = 0;

  /* readonly attribute PRInt32 roIntProp; */
  NS_IMETHOD GetRoIntProp(PRInt32 *aRoIntProp) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_TEST \
  NS_IMETHOD Say(const char *sayIt); \
  NS_IMETHOD Shout(const char *shoutIt); \
  NS_IMETHOD Poke(const char *with); \
  NS_IMETHOD Add(PRInt32 a, PRInt32 b, PRInt32 *_retval); \
  NS_IMETHOD Peek(char **_retval); \
  NS_IMETHOD Callback(testCallback *cb); \
  NS_IMETHOD GetIntProp(PRInt32 *aIntProp); \
  NS_IMETHOD SetIntProp(PRInt32 aIntProp); \
  NS_IMETHOD GetRoIntProp(PRInt32 *aRoIntProp); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_TEST(_to) \
  NS_IMETHOD Say(const char *sayIt) { return _to Say(sayIt); } \
  NS_IMETHOD Shout(const char *shoutIt) { return _to Shout(shoutIt); } \
  NS_IMETHOD Poke(const char *with) { return _to Poke(with); } \
  NS_IMETHOD Add(PRInt32 a, PRInt32 b, PRInt32 *_retval) { return _to Add(a, b, _retval); } \
  NS_IMETHOD Peek(char **_retval) { return _to Peek(_retval); } \
  NS_IMETHOD Callback(testCallback *cb) { return _to Callback(cb); } \
  NS_IMETHOD GetIntProp(PRInt32 *aIntProp) { return _to GetIntProp(aIntProp); } \
  NS_IMETHOD SetIntProp(PRInt32 aIntProp) { return _to SetIntProp(aIntProp); } \
  NS_IMETHOD GetRoIntProp(PRInt32 *aRoIntProp) { return _to GetRoIntProp(aRoIntProp); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_TEST(_to) \
  NS_IMETHOD Say(const char *sayIt) { return !_to ? NS_ERROR_NULL_POINTER : _to->Say(sayIt); } \
  NS_IMETHOD Shout(const char *shoutIt) { return !_to ? NS_ERROR_NULL_POINTER : _to->Shout(shoutIt); } \
  NS_IMETHOD Poke(const char *with) { return !_to ? NS_ERROR_NULL_POINTER : _to->Poke(with); } \
  NS_IMETHOD Add(PRInt32 a, PRInt32 b, PRInt32 *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->Add(a, b, _retval); } \
  NS_IMETHOD Peek(char **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->Peek(_retval); } \
  NS_IMETHOD Callback(testCallback *cb) { return !_to ? NS_ERROR_NULL_POINTER : _to->Callback(cb); } \
  NS_IMETHOD GetIntProp(PRInt32 *aIntProp) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetIntProp(aIntProp); } \
  NS_IMETHOD SetIntProp(PRInt32 aIntProp) { return !_to ? NS_ERROR_NULL_POINTER : _to->SetIntProp(aIntProp); } \
  NS_IMETHOD GetRoIntProp(PRInt32 *aRoIntProp) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetRoIntProp(aRoIntProp); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public test
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_TEST

  _MYCLASS_();
  virtual ~_MYCLASS_();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, test)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* void say (in string sayIt); */
NS_IMETHODIMP _MYCLASS_::Say(const char *sayIt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void shout (in string shoutIt); */
NS_IMETHODIMP _MYCLASS_::Shout(const char *shoutIt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void poke (in string with); */
NS_IMETHODIMP _MYCLASS_::Poke(const char *with)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRInt32 add (in PRInt32 a, in PRInt32 b); */
NS_IMETHODIMP _MYCLASS_::Add(PRInt32 a, PRInt32 b, PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string peek (); */
NS_IMETHODIMP _MYCLASS_::Peek(char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void callback (in testCallback cb); */
NS_IMETHODIMP _MYCLASS_::Callback(testCallback *cb)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute PRInt32 intProp; */
NS_IMETHODIMP _MYCLASS_::GetIntProp(PRInt32 *aIntProp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP _MYCLASS_::SetIntProp(PRInt32 aIntProp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRInt32 roIntProp; */
NS_IMETHODIMP _MYCLASS_::GetRoIntProp(PRInt32 *aRoIntProp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


/* starting interface:    testString */
#define TESTSTRING_IID_STR "5a1f21a2-8aa3-4147-a808-1e1a422dcb76"

#define TESTSTRING_IID \
  {0x5a1f21a2, 0x8aa3, 0x4147, \
    { 0xa8, 0x08, 0x1e, 0x1a, 0x42, 0x2d, 0xcb, 0x76 }}

class NS_NO_VTABLE testString : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(TESTSTRING_IID)

  /* void say (in string sayIt); */
  NS_IMETHOD Say(const char *sayIt) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_TESTSTRING \
  NS_IMETHOD Say(const char *sayIt); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_TESTSTRING(_to) \
  NS_IMETHOD Say(const char *sayIt) { return _to Say(sayIt); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_TESTSTRING(_to) \
  NS_IMETHOD Say(const char *sayIt) { return !_to ? NS_ERROR_NULL_POINTER : _to->Say(sayIt); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public testString
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_TESTSTRING

  _MYCLASS_();
  virtual ~_MYCLASS_();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, testString)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* void say (in string sayIt); */
NS_IMETHODIMP _MYCLASS_::Say(const char *sayIt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


/* starting interface:    testInt */
#define TESTINT_IID_STR "5a1f21a2-8aa3-4147-a808-1e1a422dcb77"

#define TESTINT_IID \
  {0x5a1f21a2, 0x8aa3, 0x4147, \
    { 0xa8, 0x08, 0x1e, 0x1a, 0x42, 0x2d, 0xcb, 0x77 }}

class NS_NO_VTABLE testInt : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(TESTINT_IID)

  /* void add (in PRInt32 a, in PRInt32 b); */
  NS_IMETHOD Add(PRInt32 a, PRInt32 b) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_TESTINT \
  NS_IMETHOD Add(PRInt32 a, PRInt32 b); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_TESTINT(_to) \
  NS_IMETHOD Add(PRInt32 a, PRInt32 b) { return _to Add(a, b); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_TESTINT(_to) \
  NS_IMETHOD Add(PRInt32 a, PRInt32 b) { return !_to ? NS_ERROR_NULL_POINTER : _to->Add(a, b); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public testInt
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_TESTINT

  _MYCLASS_();
  virtual ~_MYCLASS_();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, testInt)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* void add (in PRInt32 a, in PRInt32 b); */
NS_IMETHODIMP _MYCLASS_::Add(PRInt32 a, PRInt32 b)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_test_h__ */
