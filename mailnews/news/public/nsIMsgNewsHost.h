/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgNewsHost.idl
 */

#ifndef __nsIMsgNewsHost_h__
#define __nsIMsgNewsHost_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgNewsgroup.h" /* interface nsIMsgNewsgroup */

/* starting interface nsIMsgNewsHost */

/* {2F5041B0-939E-11d2-B7EA-00805F05FFA5} */
#define NS_IMSGNEWSHOST_IID_STR "2F5041B0-939E-11d2-B7EA-00805F05FFA5"
#define NS_IMSGNEWSHOST_IID \
  {0x2F5041B0, 0x939E, 0x11d2, \
    { 0xB7, 0xEA, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsIMsgNewsHost : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGNEWSHOST_IID;
    return iid;
  }

  /* attribute boolean supportsExtensions; */
  NS_IMETHOD IsSupportsExtensions(PRBool *aIsSupportsExtensions) = 0;
  NS_IMETHOD SetSupportsExtensions(PRBool aSupportsExtensions) = 0;

  /* void AddExtension(in string extension); */
  NS_IMETHOD AddExtension(const char *extension) = 0;

  /* boolean QueryExtension(in string extension); */
  NS_IMETHOD QueryExtension(const char *extension, PRBool *_retval) = 0;

  /* attribute boolean postingAllowed; */
  NS_IMETHOD IsPostingAllowed(PRBool *aIsPostingAllowed) = 0;
  NS_IMETHOD SetPostingAllowed(PRBool aPostingAllowed) = 0;

  /* attribute boolean pushAuth; */
  NS_IMETHOD IsPushAuth(PRBool *aIsPushAuth) = 0;
  NS_IMETHOD SetPushAuth(PRBool aPushAuth) = 0;

  /* attribute  lastUpdatedTime; */
  NS_IMETHOD GetLastUpdatedTime(PRInt64 *aLastUpdatedTime) = 0;
  NS_IMETHOD SetLastUpdatedTime(PRInt64 aLastUpdatedTime) = 0;

  /* void AddPropertyForGet(in string name, in string value); */
  NS_IMETHOD AddPropertyForGet(const char *name, const char *value) = 0;

  /* void AddSearchableGroup(in string groupname); */
  NS_IMETHOD AddSearchableGroup(const char *groupname) = 0;

  /* void AddProfileGroup(in string responseText); */
  NS_IMETHOD AddProfileGroup(const char *responseText) = 0;

  /* void AddSearchableHeader(in string headerName); */
  NS_IMETHOD AddSearchableHeader(const char *headerName) = 0;

  /* void AddSubscribedNewsgroup(in string url); */
  NS_IMETHOD AddSubscribedNewsgroup(const char *url) = 0;

  /* void GroupNotFound(in string group, in boolean opening); */
  NS_IMETHOD GroupNotFound(const char *group, PRBool opening) = 0;

  /* void AddNewNewsgroup(in string groupname, in  first, in  last, in string flags, in boolean xactiveFlags); */
  NS_IMETHOD AddNewNewsgroup(const char *groupname, PRInt32 first, PRInt32 last, const char *flags, PRBool xactiveFlags) = 0;

  /*  GetNumGroupsNeedingCounts(); */
  NS_IMETHOD GetNumGroupsNeedingCounts(PRInt32 *_retval) = 0;

  /* string GetFirstGroupNeedingCounts(); */
  NS_IMETHOD GetFirstGroupNeedingCounts(char **_retval) = 0;

  /* void DisplaySubscribedGroup(in string groupname, in  first_message, in  last_message, in  total_messages, in boolean visit_now); */
  NS_IMETHOD DisplaySubscribedGroup(const char *groupname, PRInt32 first_message, PRInt32 last_message, PRInt32 total_messages, PRBool visit_now) = 0;

  /* nsIMsgNewsgroup GetFirstGroupNeedingExtraInfo(); */
  NS_IMETHOD GetFirstGroupNeedingExtraInfo(nsIMsgNewsgroup **_retval) = 0;

  /* void SetGroupNeedsExtraInfo(in string groupname, in boolean needsExtraInfo); */
  NS_IMETHOD SetGroupNeedsExtraInfo(const char *groupname, PRBool needsExtraInfo) = 0;

  /* void GetNewsGroupAndNumberOfID(in string message_id, out nsIMsgNewsgroup group, out unsigned  message_number); */
  NS_IMETHOD GetNewsGroupAndNumberOfID(const char *message_id, nsIMsgNewsgroup **group, PRUint32 *message_number) = 0;

  /* void AddPrettyName(in string groupName, in string prettyName); */
  NS_IMETHOD AddPrettyName(const char *groupName, const char *prettyName) = 0;
};

#endif /* __nsIMsgNewsHost_h__ */
