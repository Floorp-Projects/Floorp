/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMWindowEventOwner_h__
#define nsIDOMWindowEventOwner_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "jsapi.h"


#define NS_IDOMWINDOWEVENTOWNER_IID \
 { 0xef1876f0, 0x7881, 0x11d4, \
   { 0x9a, 0x80, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74 } } 

class nsIDOMWindowEventOwner : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMWINDOWEVENTOWNER_IID; return iid; }

  NS_IMETHOD    GetOnmousedown(jsval* aOnmousedown)=0;
  NS_IMETHOD    SetOnmousedown(jsval aOnmousedown)=0;

  NS_IMETHOD    GetOnmouseup(jsval* aOnmouseup)=0;
  NS_IMETHOD    SetOnmouseup(jsval aOnmouseup)=0;

  NS_IMETHOD    GetOnclick(jsval* aOnclick)=0;
  NS_IMETHOD    SetOnclick(jsval aOnclick)=0;

  NS_IMETHOD    GetOnmouseover(jsval* aOnmouseover)=0;
  NS_IMETHOD    SetOnmouseover(jsval aOnmouseover)=0;

  NS_IMETHOD    GetOnmouseout(jsval* aOnmouseout)=0;
  NS_IMETHOD    SetOnmouseout(jsval aOnmouseout)=0;

  NS_IMETHOD    GetOnkeydown(jsval* aOnkeydown)=0;
  NS_IMETHOD    SetOnkeydown(jsval aOnkeydown)=0;

  NS_IMETHOD    GetOnkeyup(jsval* aOnkeyup)=0;
  NS_IMETHOD    SetOnkeyup(jsval aOnkeyup)=0;

  NS_IMETHOD    GetOnkeypress(jsval* aOnkeypress)=0;
  NS_IMETHOD    SetOnkeypress(jsval aOnkeypress)=0;

  NS_IMETHOD    GetOnmousemove(jsval* aOnmousemove)=0;
  NS_IMETHOD    SetOnmousemove(jsval aOnmousemove)=0;

  NS_IMETHOD    GetOnfocus(jsval* aOnfocus)=0;
  NS_IMETHOD    SetOnfocus(jsval aOnfocus)=0;

  NS_IMETHOD    GetOnblur(jsval* aOnblur)=0;
  NS_IMETHOD    SetOnblur(jsval aOnblur)=0;

  NS_IMETHOD    GetOnsubmit(jsval* aOnsubmit)=0;
  NS_IMETHOD    SetOnsubmit(jsval aOnsubmit)=0;

  NS_IMETHOD    GetOnreset(jsval* aOnreset)=0;
  NS_IMETHOD    SetOnreset(jsval aOnreset)=0;

  NS_IMETHOD    GetOnchange(jsval* aOnchange)=0;
  NS_IMETHOD    SetOnchange(jsval aOnchange)=0;

  NS_IMETHOD    GetOnselect(jsval* aOnselect)=0;
  NS_IMETHOD    SetOnselect(jsval aOnselect)=0;

  NS_IMETHOD    GetOnload(jsval* aOnload)=0;
  NS_IMETHOD    SetOnload(jsval aOnload)=0;

  NS_IMETHOD    GetOnunload(jsval* aOnunload)=0;
  NS_IMETHOD    SetOnunload(jsval aOnunload)=0;

  NS_IMETHOD    GetOnclose(jsval* aOnclose)=0;
  NS_IMETHOD    SetOnclose(jsval aOnclose)=0;

  NS_IMETHOD    GetOnabort(jsval* aOnabort)=0;
  NS_IMETHOD    SetOnabort(jsval aOnabort)=0;

  NS_IMETHOD    GetOnerror(jsval* aOnerror)=0;
  NS_IMETHOD    SetOnerror(jsval aOnerror)=0;

  NS_IMETHOD    GetOnpaint(jsval* aOnpaint)=0;
  NS_IMETHOD    SetOnpaint(jsval aOnpaint)=0;

  NS_IMETHOD    GetOndragdrop(jsval* aOndragdrop)=0;
  NS_IMETHOD    SetOndragdrop(jsval aOndragdrop)=0;

  NS_IMETHOD    GetOnresize(jsval* aOnresize)=0;
  NS_IMETHOD    SetOnresize(jsval aOnresize)=0;

  NS_IMETHOD    GetOnscroll(jsval* aOnscroll)=0;
  NS_IMETHOD    SetOnscroll(jsval aOnscroll)=0;
};


#define NS_DECL_IDOMWINDOWEVENTOWNER   \
  NS_IMETHOD    GetOnmousedown(jsval* aOnmousedown);  \
  NS_IMETHOD    SetOnmousedown(jsval aOnmousedown);  \
  NS_IMETHOD    GetOnmouseup(jsval* aOnmouseup);  \
  NS_IMETHOD    SetOnmouseup(jsval aOnmouseup);  \
  NS_IMETHOD    GetOnclick(jsval* aOnclick);  \
  NS_IMETHOD    SetOnclick(jsval aOnclick);  \
  NS_IMETHOD    GetOnmouseover(jsval* aOnmouseover);  \
  NS_IMETHOD    SetOnmouseover(jsval aOnmouseover);  \
  NS_IMETHOD    GetOnmouseout(jsval* aOnmouseout);  \
  NS_IMETHOD    SetOnmouseout(jsval aOnmouseout);  \
  NS_IMETHOD    GetOnkeydown(jsval* aOnkeydown);  \
  NS_IMETHOD    SetOnkeydown(jsval aOnkeydown);  \
  NS_IMETHOD    GetOnkeyup(jsval* aOnkeyup);  \
  NS_IMETHOD    SetOnkeyup(jsval aOnkeyup);  \
  NS_IMETHOD    GetOnkeypress(jsval* aOnkeypress);  \
  NS_IMETHOD    SetOnkeypress(jsval aOnkeypress);  \
  NS_IMETHOD    GetOnmousemove(jsval* aOnmousemove);  \
  NS_IMETHOD    SetOnmousemove(jsval aOnmousemove);  \
  NS_IMETHOD    GetOnfocus(jsval* aOnfocus);  \
  NS_IMETHOD    SetOnfocus(jsval aOnfocus);  \
  NS_IMETHOD    GetOnblur(jsval* aOnblur);  \
  NS_IMETHOD    SetOnblur(jsval aOnblur);  \
  NS_IMETHOD    GetOnsubmit(jsval* aOnsubmit);  \
  NS_IMETHOD    SetOnsubmit(jsval aOnsubmit);  \
  NS_IMETHOD    GetOnreset(jsval* aOnreset);  \
  NS_IMETHOD    SetOnreset(jsval aOnreset);  \
  NS_IMETHOD    GetOnchange(jsval* aOnchange);  \
  NS_IMETHOD    SetOnchange(jsval aOnchange);  \
  NS_IMETHOD    GetOnselect(jsval* aOnselect);  \
  NS_IMETHOD    SetOnselect(jsval aOnselect);  \
  NS_IMETHOD    GetOnload(jsval* aOnload);  \
  NS_IMETHOD    SetOnload(jsval aOnload);  \
  NS_IMETHOD    GetOnunload(jsval* aOnunload);  \
  NS_IMETHOD    SetOnunload(jsval aOnunload);  \
  NS_IMETHOD    GetOnclose(jsval* aOnclose);  \
  NS_IMETHOD    SetOnclose(jsval aOnclose);  \
  NS_IMETHOD    GetOnabort(jsval* aOnabort);  \
  NS_IMETHOD    SetOnabort(jsval aOnabort);  \
  NS_IMETHOD    GetOnerror(jsval* aOnerror);  \
  NS_IMETHOD    SetOnerror(jsval aOnerror);  \
  NS_IMETHOD    GetOnpaint(jsval* aOnpaint);  \
  NS_IMETHOD    SetOnpaint(jsval aOnpaint);  \
  NS_IMETHOD    GetOndragdrop(jsval* aOndragdrop);  \
  NS_IMETHOD    SetOndragdrop(jsval aOndragdrop);  \
  NS_IMETHOD    GetOnresize(jsval* aOnresize);  \
  NS_IMETHOD    SetOnresize(jsval aOnresize);  \
  NS_IMETHOD    GetOnscroll(jsval* aOnscroll);  \
  NS_IMETHOD    SetOnscroll(jsval aOnscroll);  \



#define NS_FORWARD_IDOMWINDOWEVENTOWNER(_to)  \
  NS_IMETHOD    GetOnmousedown(jsval* aOnmousedown) { return _to GetOnmousedown(aOnmousedown); } \
  NS_IMETHOD    SetOnmousedown(jsval aOnmousedown) { return _to SetOnmousedown(aOnmousedown); } \
  NS_IMETHOD    GetOnmouseup(jsval* aOnmouseup) { return _to GetOnmouseup(aOnmouseup); } \
  NS_IMETHOD    SetOnmouseup(jsval aOnmouseup) { return _to SetOnmouseup(aOnmouseup); } \
  NS_IMETHOD    GetOnclick(jsval* aOnclick) { return _to GetOnclick(aOnclick); } \
  NS_IMETHOD    SetOnclick(jsval aOnclick) { return _to SetOnclick(aOnclick); } \
  NS_IMETHOD    GetOnmouseover(jsval* aOnmouseover) { return _to GetOnmouseover(aOnmouseover); } \
  NS_IMETHOD    SetOnmouseover(jsval aOnmouseover) { return _to SetOnmouseover(aOnmouseover); } \
  NS_IMETHOD    GetOnmouseout(jsval* aOnmouseout) { return _to GetOnmouseout(aOnmouseout); } \
  NS_IMETHOD    SetOnmouseout(jsval aOnmouseout) { return _to SetOnmouseout(aOnmouseout); } \
  NS_IMETHOD    GetOnkeydown(jsval* aOnkeydown) { return _to GetOnkeydown(aOnkeydown); } \
  NS_IMETHOD    SetOnkeydown(jsval aOnkeydown) { return _to SetOnkeydown(aOnkeydown); } \
  NS_IMETHOD    GetOnkeyup(jsval* aOnkeyup) { return _to GetOnkeyup(aOnkeyup); } \
  NS_IMETHOD    SetOnkeyup(jsval aOnkeyup) { return _to SetOnkeyup(aOnkeyup); } \
  NS_IMETHOD    GetOnkeypress(jsval* aOnkeypress) { return _to GetOnkeypress(aOnkeypress); } \
  NS_IMETHOD    SetOnkeypress(jsval aOnkeypress) { return _to SetOnkeypress(aOnkeypress); } \
  NS_IMETHOD    GetOnmousemove(jsval* aOnmousemove) { return _to GetOnmousemove(aOnmousemove); } \
  NS_IMETHOD    SetOnmousemove(jsval aOnmousemove) { return _to SetOnmousemove(aOnmousemove); } \
  NS_IMETHOD    GetOnfocus(jsval* aOnfocus) { return _to GetOnfocus(aOnfocus); } \
  NS_IMETHOD    SetOnfocus(jsval aOnfocus) { return _to SetOnfocus(aOnfocus); } \
  NS_IMETHOD    GetOnblur(jsval* aOnblur) { return _to GetOnblur(aOnblur); } \
  NS_IMETHOD    SetOnblur(jsval aOnblur) { return _to SetOnblur(aOnblur); } \
  NS_IMETHOD    GetOnsubmit(jsval* aOnsubmit) { return _to GetOnsubmit(aOnsubmit); } \
  NS_IMETHOD    SetOnsubmit(jsval aOnsubmit) { return _to SetOnsubmit(aOnsubmit); } \
  NS_IMETHOD    GetOnreset(jsval* aOnreset) { return _to GetOnreset(aOnreset); } \
  NS_IMETHOD    SetOnreset(jsval aOnreset) { return _to SetOnreset(aOnreset); } \
  NS_IMETHOD    GetOnchange(jsval* aOnchange) { return _to GetOnchange(aOnchange); } \
  NS_IMETHOD    SetOnchange(jsval aOnchange) { return _to SetOnchange(aOnchange); } \
  NS_IMETHOD    GetOnselect(jsval* aOnselect) { return _to GetOnselect(aOnselect); } \
  NS_IMETHOD    SetOnselect(jsval aOnselect) { return _to SetOnselect(aOnselect); } \
  NS_IMETHOD    GetOnload(jsval* aOnload) { return _to GetOnload(aOnload); } \
  NS_IMETHOD    SetOnload(jsval aOnload) { return _to SetOnload(aOnload); } \
  NS_IMETHOD    GetOnunload(jsval* aOnunload) { return _to GetOnunload(aOnunload); } \
  NS_IMETHOD    SetOnunload(jsval aOnunload) { return _to SetOnunload(aOnunload); } \
  NS_IMETHOD    GetOnclose(jsval* aOnclose) { return _to GetOnclose(aOnclose); } \
  NS_IMETHOD    SetOnclose(jsval aOnclose) { return _to SetOnclose(aOnclose); } \
  NS_IMETHOD    GetOnabort(jsval* aOnabort) { return _to GetOnabort(aOnabort); } \
  NS_IMETHOD    SetOnabort(jsval aOnabort) { return _to SetOnabort(aOnabort); } \
  NS_IMETHOD    GetOnerror(jsval* aOnerror) { return _to GetOnerror(aOnerror); } \
  NS_IMETHOD    SetOnerror(jsval aOnerror) { return _to SetOnerror(aOnerror); } \
  NS_IMETHOD    GetOnpaint(jsval* aOnpaint) { return _to GetOnpaint(aOnpaint); } \
  NS_IMETHOD    SetOnpaint(jsval aOnpaint) { return _to SetOnpaint(aOnpaint); } \
  NS_IMETHOD    GetOndragdrop(jsval* aOndragdrop) { return _to GetOndragdrop(aOndragdrop); } \
  NS_IMETHOD    SetOndragdrop(jsval aOndragdrop) { return _to SetOndragdrop(aOndragdrop); } \
  NS_IMETHOD    GetOnresize(jsval* aOnresize) { return _to GetOnresize(aOnresize); } \
  NS_IMETHOD    SetOnresize(jsval aOnresize) { return _to SetOnresize(aOnresize); } \
  NS_IMETHOD    GetOnscroll(jsval* aOnscroll) { return _to GetOnscroll(aOnscroll); } \
  NS_IMETHOD    SetOnscroll(jsval aOnscroll) { return _to SetOnscroll(aOnscroll); } \


#endif // nsIDOMWindowEventOwner_h__
