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
  private:
    void *tagData; 
  
  public: 
    nsRFC822toHTMLStreamConverter();
	  virtual ~nsRFC822toHTMLStreamConverter();
     
    /* this macro defines QueryInterface, AddRef and Release for this class */
    NS_DECL_ISUPPORTS 

    /* Inherited methods for nsIStreamConverter */
    NS_IMETHOD SetOutputStream(class nsIOutputStream *);
    NS_IMETHOD SetOutputListener(class nsIStreamListener *);

    /* Inherited methods for nsIStreamListener */
    NS_IMETHOD OnStartBinding(class nsIURL *,const char *);
    NS_IMETHOD OnProgress(class nsIURL *,unsigned int,unsigned int);
    NS_IMETHOD OnStatus(class nsIURL *,const unsigned short *);
    NS_IMETHOD OnStopBinding(class nsIURL *,unsigned int,const unsigned short *);
    NS_IMETHOD GetBindInfo(class nsIURL *,struct nsStreamBindingInfo *);
    NS_IMETHOD OnDataAvailable(class nsIURL *,class nsIInputStream *,unsigned int);
}; 

/* this function will be used by the factory to generate an RFC822 - HTML Converter....*/
extern nsresult NS_NewRFC822HTMLConverter(nsIStreamConverter** aInstancePtrResult);

#endif /* nsRFC822toHTMLStreamConverter_h_ */ 
