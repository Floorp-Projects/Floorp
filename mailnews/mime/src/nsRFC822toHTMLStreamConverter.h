#ifndef nsRFC822toHTMLStreamConverter_h_ 
#define nsRFC822toHTMLStreamConverter_h_ 

#include "nsIStreamConverter.h" 

// 
// A specific stream converter will be specified for each format-in/format-out 
// pairing. 
// 

// {22DB1685-AA68-11d2-8809-00805F5A1FB8} 
#define NS_RFC822_HTML_STREAM_CONVERTER_CID \ 
  { 0x22db1685, 0xaa68, 0x11d2, \ 
  { 0x88, 0x9, 0x0, 0x80, 0x5f, 0x5a, 0x1f, 0xb8 } }; 

class nsRFC822toHTMLStreamConverter : public nsIStreamConverter { 
public: 
    void *tagData; 
}; 

#endif /* nsRFC822toHTMLStreamConverter_h_ */ 
