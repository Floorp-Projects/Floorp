/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIDebugPlugin.idl
 */

#ifndef __gen_nsIDebugPlugin_h__
#define __gen_nsIDebugPlugin_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    nsIDebugPlugin */
#define NS_IDEBUGPLUGIN_IID_STR "482e1890-1fe5-11d5-9cf8-0060b0fbd8ac"

#define NS_IDEBUGPLUGIN_IID \
  {0x482e1890, 0x1fe5, 0x11d5, \
    { 0x9c, 0xf8, 0x00, 0x60, 0xb0, 0xfb, 0xd8, 0xac }}

class NS_NO_VTABLE nsIDebugPlugin : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDEBUGPLUGIN_IID)

  /* readonly attribute string version; */
  NS_IMETHOD GetVersion(char * *aVersion) = 0;

  /* void DumpLayout (in nsISupports aWindow, in wstring aFilePath, in wstring aFileName); */
  NS_IMETHOD DumpLayout(nsISupports *aWindow, const PRUnichar *aFilePath, const PRUnichar *aFileName) = 0;

  /* void StartDirectorySearch (in string aFilePath); */
  NS_IMETHOD StartDirectorySearch(const char *aFilePath) = 0;

  /* void GetNextFileInDirectory ([retval] out string aFileName); */
  NS_IMETHOD GetNextFileInDirectory(char **aFileName) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIDEBUGPLUGIN \
  NS_IMETHOD GetVersion(char * *aVersion); \
  NS_IMETHOD DumpLayout(nsISupports *aWindow, const PRUnichar *aFilePath, const PRUnichar *aFileName); \
  NS_IMETHOD StartDirectorySearch(const char *aFilePath); \
  NS_IMETHOD GetNextFileInDirectory(char **aFileName); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIDEBUGPLUGIN(_to) \
  NS_IMETHOD GetVersion(char * *aVersion) { return _to GetVersion(aVersion); } \
  NS_IMETHOD DumpLayout(nsISupports *aWindow, const PRUnichar *aFilePath, const PRUnichar *aFileName) { return _to DumpLayout(aWindow, aFilePath, aFileName); } \
  NS_IMETHOD StartDirectorySearch(const char *aFilePath) { return _to StartDirectorySearch(aFilePath); } \
  NS_IMETHOD GetNextFileInDirectory(char **aFileName) { return _to GetNextFileInDirectory(aFileName); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSIDEBUGPLUGIN(_to) \
  NS_IMETHOD GetVersion(char * *aVersion) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetVersion(aVersion); } \
  NS_IMETHOD DumpLayout(nsISupports *aWindow, const PRUnichar *aFilePath, const PRUnichar *aFileName) { return !_to ? NS_ERROR_NULL_POINTER : _to->DumpLayout(aWindow, aFilePath, aFileName); } \
  NS_IMETHOD StartDirectorySearch(const char *aFilePath) { return !_to ? NS_ERROR_NULL_POINTER : _to->StartDirectorySearch(aFilePath); } \
  NS_IMETHOD GetNextFileInDirectory(char **aFileName) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetNextFileInDirectory(aFileName); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsDebugPlugin : public nsIDebugPlugin
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDEBUGPLUGIN

  nsDebugPlugin();
  virtual ~nsDebugPlugin();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsDebugPlugin, nsIDebugPlugin)

nsDebugPlugin::nsDebugPlugin()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsDebugPlugin::~nsDebugPlugin()
{
  /* destructor code */
}

/* readonly attribute string version; */
NS_IMETHODIMP nsDebugPlugin::GetVersion(char * *aVersion)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void DumpLayout (in nsISupports aWindow, in wstring aFilePath, in wstring aFileName); */
NS_IMETHODIMP nsDebugPlugin::DumpLayout(nsISupports *aWindow, const PRUnichar *aFilePath, const PRUnichar *aFileName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void StartDirectorySearch (in string aFilePath); */
NS_IMETHODIMP nsDebugPlugin::StartDirectorySearch(const char *aFilePath)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GetNextFileInDirectory ([retval] out string aFileName); */
NS_IMETHODIMP nsDebugPlugin::GetNextFileInDirectory(char **aFileName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsIDebugPlugin_h__ */
