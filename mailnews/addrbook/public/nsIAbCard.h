/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIAbCard.idl
 */

#ifndef __gen_nsIAbCard_h__
#define __gen_nsIAbCard_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIAbBase.h" /* interface nsIAbBase */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIAbListener.h" /* interface nsIAbListener */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIAbCard */

/* {FA5C977F-04C8-11d3-A2EB-001083003D0C} */
#define NS_IABCARD_IID_STR "FA5C977F-04C8-11d3-A2EB-001083003D0C"
#define NS_IABCARD_IID \
  {0xFA5C977F, 0x04C8, 0x11d3, \
    { 0xA2, 0xEB, 0x00, 0x10, 0x83, 0x00, 0x3D, 0x0C }}

class nsIAbCard : public nsIAbBase {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IABCARD_IID)

  /* attribute string personName; */
  NS_IMETHOD GetPersonName(char * *aPersonName) = 0;
  NS_IMETHOD SetPersonName(char * aPersonName) = 0;

  /* attribute string listName; */
  NS_IMETHOD GetListName(char * *aListName) = 0;
  NS_IMETHOD SetListName(char * aListName) = 0;

  /* attribute string email; */
  NS_IMETHOD GetEmail(char * *aEmail) = 0;
  NS_IMETHOD SetEmail(char * aEmail) = 0;

  /* attribute string workPhone; */
  NS_IMETHOD GetWorkPhone(char * *aWorkPhone) = 0;
  NS_IMETHOD SetWorkPhone(char * aWorkPhone) = 0;

  /* attribute string city; */
  NS_IMETHOD GetCity(char * *aCity) = 0;
  NS_IMETHOD SetCity(char * aCity) = 0;

  /* attribute string organization; */
  NS_IMETHOD GetOrganization(char * *aOrganization) = 0;
  NS_IMETHOD SetOrganization(char * aOrganization) = 0;

  /* attribute string nickname; */
  NS_IMETHOD GetNickname(char * *aNickname) = 0;
  NS_IMETHOD SetNickname(char * aNickname) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIAbCard *priv);
#endif
};

#endif /* __gen_nsIAbCard_h__ */
