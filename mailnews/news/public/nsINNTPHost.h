/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPHost.idl
 */

#ifndef __gen_nsINNTPHost_h__
#define __gen_nsINNTPHost_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsINNTPNewsgroup.h" /* interface nsINNTPNewsgroup */
#include "nsINNTPNewsgroupList.h" /* interface nsINNTPNewsgroupList */


/* starting interface nsINNTPHost */

/* {ADFB3740-AA57-11d2-B7ED-00805F05FFA5} */
#define NS_INNTPHOST_IID_STR "ADFB3740-AA57-11d2-B7ED-00805F05FFA5"
#define NS_INNTPHOST_IID \
  {0xADFB3740, 0xAA57, 0x11d2, \
    { 0xB7, 0xED, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsINNTPHost {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_INNTPHOST_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD IsSupportsExtensions(PRBool *aIsSupportsExtensions) = 0;
  NS_IMETHOD SetSupportsExtensions(PRBool aSupportsExtensions) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddExtension(const char *extension) = 0;

  /*  <IDL>  */
  NS_IMETHOD QueryExtension(const char *extension, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsPostingAllowed(PRBool *aIsPostingAllowed) = 0;
  NS_IMETHOD SetPostingAllowed(PRBool aPostingAllowed) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsPushAuth(PRBool *aIsPushAuth) = 0;
  NS_IMETHOD SetPushAuth(PRBool aPushAuth) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetLastUpdatedTime(PRInt64 *aLastUpdatedTime) = 0;
  NS_IMETHOD SetLastUpdatedTime(PRInt64 aLastUpdatedTime) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNewsgroupList(const char *groupname, nsINNTPNewsgroupList **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD FindNewsgroup(const char *groupname, PRBool create, nsINNTPNewsgroup **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddPropertyForGet(const char *name, const char *value) = 0;

  /*  <IDL>  */
  NS_IMETHOD QueryPropertyForGet(const char *name, char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddSearchableGroup(const char *groupname) = 0;

  /*  <IDL>  */
  NS_IMETHOD QuerySearchableGroup(const char *groupname, char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddVirtualGroup(const char *responseText) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetIsVirtualGroup(const char *groupname, PRBool isVirtual) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetIsVirtualGroup(const char *groupname, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddSearchableHeader(const char *headerName) = 0;

  /*  <IDL>  */
  NS_IMETHOD QuerySearchableHeader(const char *headerName, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddSubscribedNewsgroup(const char *url) = 0;

  /*  <IDL>  */
  NS_IMETHOD GroupNotFound(const char *group, PRBool opening) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddNewNewsgroup(const char *groupname, PRInt32 first, PRInt32 last, const char *flags, PRBool xactiveFlags) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNumGroupsNeedingCounts(PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFirstGroupNeedingCounts(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD DisplaySubscribedGroup(const char *groupname, PRInt32 first_message, PRInt32 last_message, PRInt32 total_messages, PRBool visit_now) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFirstGroupNeedingExtraInfo(nsINNTPNewsgroup **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetGroupNeedsExtraInfo(const char *groupname, PRBool needsExtraInfo) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNewsGroupAndNumberOfID(const char *message_id, nsINNTPNewsgroup **group, PRUint32 *message_number) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetPrettyName(const char *groupName, const char *prettyName) = 0;

  /*  <IDL>  */
  NS_IMETHOD LoadNewsrc() = 0;

  /*  <IDL>  */
  NS_IMETHOD WriteNewsrc() = 0;

  /*  <IDL>  */
  NS_IMETHOD WriteIfDirty() = 0;

  /*  <IDL>  */
  NS_IMETHOD MarkDirty() = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNewsRCFilename(char * *aNewsRCFilename) = 0;
  NS_IMETHOD SetNewsRCFilename(char * aNewsRCFilename) = 0;

  /*  <IDL>  */
  NS_IMETHOD FindGroup(const char *name, nsINNTPNewsgroup **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddGroup(const char *groupname, nsINNTPNewsgroup **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD RemoveGroup(const char *groupName) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetDbDirName(char * *aDbDirName) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetGroupList(char **_retval) = 0;
};

#endif /* __gen_nsINNTPHost_h__ */
