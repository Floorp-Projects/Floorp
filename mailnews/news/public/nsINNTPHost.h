/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPHost.idl
 */

#ifndef __gen_nsINNTPHost_h__
#define __gen_nsINNTPHost_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsINNTPNewsgroup.h" /* interface nsINNTPNewsgroup */
#include "nsID.h" /* interface nsID */
#include "nsINNTPNewsgroupList.h" /* interface nsINNTPNewsgroupList */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsID.h"
#include "nsError.h"


/* starting interface nsINNTPHost */

/* {ADFB3740-AA57-11d2-B7ED-00805F05FFA5} */
#define NS_INNTPHOST_IID_STR "ADFB3740-AA57-11d2-B7ED-00805F05FFA5"
#define NS_INNTPHOST_IID \
  {0xADFB3740, 0xAA57, 0x11d2, \
    { 0xB7, 0xED, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsINNTPHost : public nsISupports {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_INNTPHOST_IID;
    return iid;
  }

  /* attribute boolean supportsExtensions; */
  NS_IMETHOD GetSupportsExtensions(PRBool *aSupportsExtensions) = 0;
  NS_IMETHOD SetSupportsExtensions(PRBool aSupportsExtensions) = 0;

  /* void AddExtension (in string extension); */
  NS_IMETHOD AddExtension(char *extension) = 0;

  /* boolean QueryExtension (in string extension); */
  NS_IMETHOD QueryExtension(char *extension, PRBool *_retval) = 0;

  /* attribute boolean postingAllowed; */
  NS_IMETHOD GetPostingAllowed(PRBool *aPostingAllowed) = 0;
  NS_IMETHOD SetPostingAllowed(PRBool aPostingAllowed) = 0;

  /* attribute boolean pushAuth; */
  NS_IMETHOD GetPushAuth(PRBool *aPushAuth) = 0;
  NS_IMETHOD SetPushAuth(PRBool aPushAuth) = 0;

  /* attribute long long lastUpdatedTime; */
  NS_IMETHOD GetLastUpdatedTime(PRInt64 *aLastUpdatedTime) = 0;
  NS_IMETHOD SetLastUpdatedTime(PRInt64 aLastUpdatedTime) = 0;

  /* nsINNTPNewsgroupList GetNewsgroupList (in string groupname); */
  NS_IMETHOD GetNewsgroupList(char *groupname, nsINNTPNewsgroupList **_retval) = 0;

  /* nsINNTPNewsgroup FindNewsgroup (in string groupname, in boolean create); */
  NS_IMETHOD FindNewsgroup(char *groupname, PRBool create, nsINNTPNewsgroup **_retval) = 0;

  /* void AddPropertyForGet (in string name, in string value); */
  NS_IMETHOD AddPropertyForGet(char *name, char *value) = 0;

  /* string QueryPropertyForGet (in string name); */
  NS_IMETHOD QueryPropertyForGet(char *name, char **_retval) = 0;

  /* void AddSearchableGroup (in string groupname); */
  NS_IMETHOD AddSearchableGroup(char *groupname) = 0;

  /* boolean QuerySearchableGroup (in string groupname); */
  NS_IMETHOD QuerySearchableGroup(char *groupname, PRBool *_retval) = 0;

  /* void AddVirtualGroup (in string responseText); */
  NS_IMETHOD AddVirtualGroup(char *responseText) = 0;

  /* void SetIsVirtualGroup (in string groupname, in boolean isVirtual); */
  NS_IMETHOD SetIsVirtualGroup(char *groupname, PRBool isVirtual) = 0;

  /* boolean GetIsVirtualGroup (in string groupname); */
  NS_IMETHOD GetIsVirtualGroup(char *groupname, PRBool *_retval) = 0;

  /* void AddSearchableHeader (in string headerName); */
  NS_IMETHOD AddSearchableHeader(char *headerName) = 0;

  /* boolean QuerySearchableHeader (in string headerName); */
  NS_IMETHOD QuerySearchableHeader(char *headerName, PRBool *_retval) = 0;

  /* void GroupNotFound (in string group, in boolean opening); */
  NS_IMETHOD GroupNotFound(char *group, PRBool opening) = 0;

  /* void AddNewNewsgroup (in string groupname, in long first, in long last, in string flags, in boolean xactiveFlags); */
  NS_IMETHOD AddNewNewsgroup(char *groupname, PRInt32 first, PRInt32 last, char *flags, PRBool xactiveFlags) = 0;

  /* long GetNumGroupsNeedingCounts (); */
  NS_IMETHOD GetNumGroupsNeedingCounts(PRInt32 *_retval) = 0;

  /* string GetFirstGroupNeedingCounts (); */
  NS_IMETHOD GetFirstGroupNeedingCounts(char **_retval) = 0;

  /* void DisplaySubscribedGroup (in string groupname, in long first_message, in long last_message, in long total_messages, in boolean visit_now); */
  NS_IMETHOD DisplaySubscribedGroup(char *groupname, PRInt32 first_message, PRInt32 last_message, PRInt32 total_messages, PRBool visit_now) = 0;

  /* string GetFirstGroupNeedingExtraInfo (); */
  NS_IMETHOD GetFirstGroupNeedingExtraInfo(char **_retval) = 0;

  /* void SetGroupNeedsExtraInfo (in string groupname, in boolean needsExtraInfo); */
  NS_IMETHOD SetGroupNeedsExtraInfo(char *groupname, PRBool needsExtraInfo) = 0;

  /* void GetNewsgroupAndNumberOfID (in string message_id, out nsINNTPNewsgroup group, out unsigned long message_number); */
  NS_IMETHOD GetNewsgroupAndNumberOfID(char *message_id, nsINNTPNewsgroup **group, PRUint32 *message_number) = 0;

  /* void SetPrettyName (in string groupName, in string prettyName); */
  NS_IMETHOD SetPrettyName(char *groupName, char *prettyName) = 0;

  /* void LoadNewsrc (); */
  NS_IMETHOD LoadNewsrc() = 0;

  /* void WriteNewsrc (); */
  NS_IMETHOD WriteNewsrc() = 0;

  /* void WriteIfDirty (); */
  NS_IMETHOD WriteIfDirty() = 0;

  /* void MarkDirty (); */
  NS_IMETHOD MarkDirty() = 0;

  /* attribute string newsRCFilename; */
  NS_IMETHOD GetNewsRCFilename(char * *aNewsRCFilename) = 0;
  NS_IMETHOD SetNewsRCFilename(char * aNewsRCFilename) = 0;

  /* nsINNTPNewsgroup FindGroup (in string name); */
  NS_IMETHOD FindGroup(char *name, nsINNTPNewsgroup **_retval) = 0;

  /* nsINNTPNewsgroup AddGroup (in string groupname); */
  NS_IMETHOD AddGroup(char *groupname, nsINNTPNewsgroup **_retval) = 0;

  /* void RemoveGroupByName (in string groupName); */
  NS_IMETHOD RemoveGroupByName(char *groupName) = 0;

  /* void RemoveGroup (in nsINNTPNewsgroup group); */
  NS_IMETHOD RemoveGroup(nsINNTPNewsgroup *group) = 0;

  /* readonly attribute string dbDirName; */
  NS_IMETHOD GetDbDirName(char * *aDbDirName) = 0;

  /* string GetGroupList (); */
  NS_IMETHOD GetGroupList(char **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsINNTPHost *priv);
#endif
};

#endif /* __gen_nsINNTPHost_h__ */
