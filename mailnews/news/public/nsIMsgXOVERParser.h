/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgXOVERParser.idl
 */

#ifndef __nsIMsgXOVERParser_h__
#define __nsIMsgXOVERParser_h__

#include "nsISupports.h" /* interface nsISupports */

/* starting interface nsIMsgXOVERParser */

/* {E628ED19-9452-11d2-B7EA-00805F05FFA5} */
#define NS_IMSGXOVERPARSER_IID_STR "E628ED19-9452-11d2-B7EA-00805F05FFA5"
#define NS_IMSGXOVERPARSER_IID \
  {0xE628ED19, 0x9452, 0x11d2, \
    { 0xB7, 0xEA, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsIMsgXOVERParser : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGXOVERPARSER_IID;
    return iid;
  }

  /* void Init(in string hostname, in string groupname, in  first_message, in  last_message, in  oldest_message, in  newest_message); */
  NS_IMETHOD Init(const char *hostname, const char *groupname, PRInt32 first_message, PRInt32 last_message, PRInt32 oldest_message, PRInt32 newest_message) = 0;

  /* void Process(in string line, out  status); */
  NS_IMETHOD Process(const char *line, PRInt32 *status) = 0;

  /* void ProcessNonXOVER(in string line); */
  NS_IMETHOD ProcessNonXOVER(const char *line) = 0;

  /* void Reset(); */
  NS_IMETHOD Reset() = 0;

  /* void Finish(in  status, out  newstatus); */
  NS_IMETHOD Finish(PRInt32 status, PRInt32 *newstatus) = 0;

  /* void ClearState(); */
  NS_IMETHOD ClearState() = 0;
};

#endif /* __nsIMsgXOVERParser_h__ */
