#include <stdio.h>
#include "nsRepository.h" 
#include "nsRFC822toHTMLStreamConverter.h"
#include "nsMimeObjectClassAccess.h"

static NS_DEFINE_IID(kIStreamConverterIID, NS_ISTREAM_CONVERTER_IID);
static NS_DEFINE_CID(kRFC822toHTMLStreamConverterCID, NS_RFC822_HTML_STREAM_CONVERTER_CID); 

static NS_DEFINE_IID(kIMimeObjectClassAccessIID, NS_IMIME_OBJECT_CLASS_ACCESS_IID);
static NS_DEFINE_CID(kMimeObjectClassAccessCID, NS_MIME_OBJECT_CLASS_ACCESS_CID); 

/* 
 * This is just a testing for libmime. All I'm doing is loading a component,
 * and querying it for a particular interface. It is its only purpose / use....
 */
int main(int argc, char *argv[]) 
{ 
  nsRFC822toHTMLStreamConverter *sample; 
  nsMimeObjectClassAccess	*objAccess;
  
  // register our dll
  nsRepository::RegisterFactory(kRFC822toHTMLStreamConverterCID, 
                                "mime.dll", PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kMimeObjectClassAccessCID,
                                "mime.dll", PR_FALSE, PR_FALSE);
  
  nsresult res = nsRepository::CreateInstance(kRFC822toHTMLStreamConverterCID, 
                    NULL, kIStreamConverterIID, (void **) &sample); 
  if (res == NS_OK && sample) 
  { 
    void *stream;

    printf("We succesfully obtained a nsRFC822toHTMLStreamConverter interface....\n");
    sample->SetOutputStream((nsIOutputStream *)stream);
    printf("Releasing the interface now...\n");
    sample->Release(); 
  } 

  printf("Time for try the nsMimeObjectClassAccess class...\n");
  res = nsRepository::CreateInstance(kMimeObjectClassAccessCID, 
                    NULL, kIMimeObjectClassAccessIID, 
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
