/* Insert copyright and license here 1995 */

#include "msgCore.h"

#include "nsMsgPost.h"
#include "nsMsgCompFields.h"

static NS_DEFINE_IID(kIMsgPost, NS_IMSGPOST_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsresult NS_NewMsgPost(const nsIID &aIID, void ** aInstancePtrResult)
{
  /* note this new macro for assertions...they can take a string describing the assertion */
  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull != aInstancePtrResult)
    {
      nsMsgPost* pPost = new nsMsgPost();
      if (pPost)
	return pPost->QueryInterface(kIMsgPost, aInstancePtrResult);
      else
	return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
    }
  else
    return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

nsMsgPost::nsMsgPost()
{
	NS_INIT_REFCNT();
}

nsMsgPost::~nsMsgPost()
{
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgPost, nsIMsgPost::GetIID());

nsresult 
nsMsgPost::PostMessage(nsIMsgCompFields *fields)
{
	const char* pBody;
	PRInt32 nBodyLength;

	if (fields) {
		pBody = ((nsMsgCompFields *)fields)->GetBody();
		if (pBody)
			nBodyLength = PL_strlen(pBody);
		else
			nBodyLength = 0;

		printf("PostMessage to: %s\n", ((nsMsgCompFields *)fields)->GetNewsgroups());
		printf("subject: %s\n", ((nsMsgCompFields *)fields)->GetSubject());
		printf("\n%s", pBody);

	}
	return NS_OK;
}
