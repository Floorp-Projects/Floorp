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

/* starting interface:    nsINNTPHost */

/* {6b128da0-d74f-11d2-b7f9-00805f05ffa5} */
#define NS_INNTPHOST_IID_STR "6b128da0-d74f-11d2-b7f9-00805f05ffa5"
#define NS_INNTPHOST_IID \
  {0x6b128da0, 0xd74f, 0x11d2, \
    { 0xb7, 0xf9, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsINNTPHost : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INNTPHOST_IID)

  /* attribute boolean supportsExtensions; */
  NS_IMETHOD GetSupportsExtensions(PRBool *aSupportsExtensions) = 0;
  NS_IMETHOD SetSupportsExtensions(PRBool aSupportsExtensions) = 0;

  /* void AddExtension (in string extension); */
  NS_IMETHOD AddExtension(const char *extension) = 0;

  /* boolean QueryExtension (in string extension); */
  NS_IMETHOD QueryExtension(const char *extension, PRBool *_retval) = 0;

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
  NS_IMETHOD GetNewsgroupList(const char *groupname, nsINNTPNewsgroupList **_retval) = 0;

  /* nsINNTPNewsgroup FindNewsgroup (in string groupname, in boolean create); */
  NS_IMETHOD FindNewsgroup(const char *groupname, PRBool create, nsINNTPNewsgroup **_retval) = 0;

  /* void AddPropertyForGet (in string name, in string value); */
  NS_IMETHOD AddPropertyForGet(const char *name, const char *value) = 0;

  /* string QueryPropertyForGet (in string name); */
  NS_IMETHOD QueryPropertyForGet(const char *name, char **_retval) = 0;

  /* void AddSearchableGroup (in string groupname); */
  NS_IMETHOD AddSearchableGroup(const char *groupname) = 0;

  /* boolean QuerySearchableGroup (in string groupname); */
  NS_IMETHOD QuerySearchableGroup(const char *groupname, PRBool *_retval) = 0;

  /* void AddVirtualGroup (in string responseText); */
  NS_IMETHOD AddVirtualGroup(const char *responseText) = 0;

  /* void SetIsVirtualGroup (in string groupname, in boolean isVirtual); */
  NS_IMETHOD SetIsVirtualGroup(const char *groupname, PRBool isVirtual) = 0;

  /* boolean GetIsVirtualGroup (in string groupname); */
  NS_IMETHOD GetIsVirtualGroup(const char *groupname, PRBool *_retval) = 0;

  /* void AddSearchableHeader (in string headerName); */
  NS_IMETHOD AddSearchableHeader(const char *headerName) = 0;

  /* boolean QuerySearchableHeader (in string headerName); */
  NS_IMETHOD QuerySearchableHeader(const char *headerName, PRBool *_retval) = 0;

  /* void GroupNotFound (in string group, in boolean opening); */
  NS_IMETHOD GroupNotFound(const char *group, PRBool opening) = 0;

  /* void AddNewNewsgroup (in string groupname, in long first, in long last, in string flags, in boolean xactiveFlags); */
  NS_IMETHOD AddNewNewsgroup(const char *groupname, PRInt32 first, PRInt32 last, const char *flags, PRBool xactiveFlags) = 0;

  /* long GetNumGroupsNeedingCounts (); */
  NS_IMETHOD GetNumGroupsNeedingCounts(PRInt32 *_retval) = 0;

  /* string GetFirstGroupNeedingCounts (); */
  NS_IMETHOD GetFirstGroupNeedingCounts(char **_retval) = 0;

  /* void DisplaySubscribedGroup (in string groupname, in long first_message, in long last_message, in long total_messages, in boolean visit_now); */
  NS_IMETHOD DisplaySubscribedGroup(const char *groupname, PRInt32 first_message, PRInt32 last_message, PRInt32 total_messages, PRBool visit_now) = 0;

  /* string GetFirstGroupNeedingExtraInfo (); */
  NS_IMETHOD GetFirstGroupNeedingExtraInfo(char **_retval) = 0;

  /* void SetGroupNeedsExtraInfo (in string groupname, in boolean needsExtraInfo); */
  NS_IMETHOD SetGroupNeedsExtraInfo(const char *groupname, PRBool needsExtraInfo) = 0;

  /* void GetNewsgroupAndNumberOfID (in string message_id, out nsINNTPNewsgroup group, out unsigned long message_number); */
  NS_IMETHOD GetNewsgroupAndNumberOfID(const char *message_id, nsINNTPNewsgroup **group, PRUint32 *message_number) = 0;

  /* void SetPrettyName (in string groupName, in string prettyName); */
  NS_IMETHOD SetPrettyName(const char *groupName, const char *prettyName) = 0;

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
  NS_IMETHOD FindGroup(const char *name, nsINNTPNewsgroup **_retval) = 0;

  /* nsINNTPNewsgroup AddGroup (in string groupname); */
  NS_IMETHOD AddGroup(const char *groupname, nsINNTPNewsgroup **_retval) = 0;

  /* void RemoveGroupByName (in string groupName); */
  NS_IMETHOD RemoveGroupByName(const char *groupName) = 0;

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
