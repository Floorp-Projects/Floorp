#ifndef nsSpamSettings_h__
#define nsSpamSettings_h__

#include "nsCOMPtr.h"
#include "nsISpamSettings.h"
#include "nsIFileSpec.h"
#include "nsIOutputStream.h"
#include "nsIMsgIncomingServer.h"

class nsSpamSettings : public nsISpamSettings
{
public:
  nsSpamSettings();
  virtual ~nsSpamSettings();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISPAMSETTINGS

private:
  nsCOMPtr <nsIMsgIncomingServer> mServer;  // make a weak ref?

  PRInt32 mLevel; 

  PRBool mMoveOnSpam;
  PRInt32 mMoveTargetMode;
  nsCString mActionTargetAccount;
  nsCString mActionTargetFolder;

  PRBool mPurge;
  PRInt32 mPurgeInterval;

  PRBool mUseWhiteList;
  nsCString mWhiteListAbURI;
  
  PRBool mLoggingEnabled;
  nsCOMPtr <nsIOutputStream> mLogStream;
  nsCString mLogURL;
  nsresult GetLogFileSpec(nsIFileSpec **aFileSpec);
  nsresult TruncateLog();
};

#endif /* nsSpamSettings_h__ */
