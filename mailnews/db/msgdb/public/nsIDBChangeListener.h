// change listener interface
#ifndef _nsIDBChangeListener_h
#define _nsIDBChangeListener_h
#include "nsISupports.h"

#define NS_IDBCHANGELISTENER_IID          \
{ 0xad0f7f90, 0xbaff, 0x11d2, { 0x8d, 0x67, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0x17}}


class nsIDBChangeListener : public nsISupports
{
public:
    static const nsIID& IID(void) { static nsIID iid = NS_IDBCHANGELISTENER_IID; return iid; }
  
    NS_IMETHOD OnKeyChange(nsMsgKey aKeyChanged, PRInt32 aFlags, 
                           nsIDBChangeListener * aInstigator) = 0;
    NS_IMETHOD OnKeyDeleted(nsMsgKey aKeyChanged, PRInt32 aFlags, 
                            nsIDBChangeListener * aInstigator) = 0;
    NS_IMETHOD OnKeyAdded(nsMsgKey aKeyChanged, PRInt32 aFlags, 
                          nsIDBChangeListener * aInstigator) = 0;
    NS_IMETHOD OnAnnouncerGoingAway(nsIDBChangeAnnouncer * instigator) = 0;
};

#endif
