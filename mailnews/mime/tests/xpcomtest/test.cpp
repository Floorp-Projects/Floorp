#include <stdio.h>
#include "nsIComponentManager.h" 
#include "nsMimeObjectClassAccess.h"

static NS_DEFINE_CID(kMimeObjectClassAccessCID, NS_MIME_OBJECT_CLASS_ACCESS_CID); 

/* 
 * This is just a testing for libmime. All I'm doing is loading a component,
 * and querying it for a particular interface. It is its only purpose / use....
 */
int main(int argc, char *argv[]) 
{ 
  nsresult                      res;
  nsMimeObjectClassAccess	*objAccess;
  
  // register our dll
  nsComponentManager::RegisterComponent(kMimeObjectClassAccessCID, NULL, NULL,
                                "mime.dll", PR_FALSE, PR_FALSE);
  
  printf("Time for try the nsMimeObjectClassAccess class...\n");
  res = nsComponentManager::CreateInstance(kMimeObjectClassAccessCID, 
                    NULL, nsIMimeObjectClassAccess::GetIID(), 
                    (void **) &objAccess); 
  if (res == NS_OK && objAccess) 
  { 
  void *ptr;
  
    printf("We succesfully obtained a nsMimeObjectClassAccess interface....\n");
    if (objAccess->GetmimeInlineTextClass(&ptr) == NS_OK)
      printf("Text Class Pointer = %x\n", ptr);
    else
      printf("Failed on XP-COM call\n");
    
    printf("Releasing the interface now...\n");
    objAccess->Release(); 
  } 
  return 0;  
}
