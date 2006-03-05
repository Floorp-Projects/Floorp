/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

#include "prenv.h"
#include "nsISupportsUtils.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIDocument.h"
#include "nsIDocumentLoader.h"
#include "nsIWebProgressListener.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsJavaDOMImpl.h"

#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"

#if defined(DEBUG)
#include <stdio.h>
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNamedNodeMap.h"
static void dump_document(nsIDOMDocument* dom, const char* urlSpec);
static void dump_node(FILE* of, nsIDOMNode* node, int indent, 
		      PRBool isMapNode);
static void dump_map(FILE* of, nsIDOMNamedNodeMap* map, int indent);
static char* strip_whitespace(const PRUnichar* input, int length);
static const char* describe_type(int type);
#endif

#ifdef JAVA_DOM_OJI_ENABLE
#include "ProxyJNI.h"
#include "nsIServiceManager.h"
nsJVMManager* nsJavaDOMImpl::jvmManager = NULL;
JavaDOMSecurityContext* nsJavaDOMImpl::securityContext = NULL;
static NS_DEFINE_CID(kJVMManagerCID,NS_JVMMANAGER_CID);
#else
JavaVM* nsJavaDOMImpl::jvm = NULL;
#endif

static NS_DEFINE_IID(kIDocShellIID, NS_IDOCSHELL_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIJavaDOMIID, NS_IJAVADOM_IID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIWebProgressListenerIID, NS_IWEBPROGRESSLISTENER_IID);

NS_IMPL_ADDREF(nsJavaDOMImpl)
NS_IMPL_RELEASE(nsJavaDOMImpl)

jclass nsJavaDOMImpl::domAccessorClass = NULL;

jmethodID nsJavaDOMImpl::startURLLoadMID = NULL;
jmethodID nsJavaDOMImpl::endURLLoadMID = NULL;
jmethodID nsJavaDOMImpl::progressURLLoadMID = NULL;
jmethodID nsJavaDOMImpl::statusURLLoadMID = NULL;
jmethodID nsJavaDOMImpl::startDocumentLoadMID = NULL;
jmethodID nsJavaDOMImpl::endDocumentLoadMID = NULL;

#define NS_JAVADOM_PROGID \
"component://netscape/blackwood/java-dom"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJavaDOMImpl)

static  nsModuleComponentInfo components[] = 
{
    {
        "Java DOM",
        NS_JAVADOM_CID,
        NS_JAVADOM_PROGID,
        nsJavaDOMImplConstructor
    }
};

NS_IMPL_NSGETMODULE("JavaDOMModule",components);

NS_IMETHODIMP nsJavaDOMImpl::QueryInterface(REFNSIID aIID, void** aInstance)
{
  if (NULL == aInstance)
    return NS_ERROR_NULL_POINTER;

  *aInstance = NULL;

  if (aIID.Equals(kIJavaDOMIID)) {
    *aInstance = (void*) ((nsIJavaDOM*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIWebProgressListenerIID)) {
    *aInstance = (void*) ((nsIWebProgressListener*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstance = (void*) ((nsISupports*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsJavaDOMImpl::nsJavaDOMImpl() 
{
  NS_INIT_ISUPPORTS();
}

PRBool nsJavaDOMImpl::Init(JNIEnv** jniEnv) {
  JNIEnv* env = GetJNIEnv();
  if (!env) return PR_FALSE;
  *jniEnv = env;

  if (!domAccessorClass) {
    domAccessorClass = env->FindClass("org/mozilla/dom/DOMAccessor");
    if (!domAccessorClass) return PR_FALSE;
    domAccessorClass = (jclass) env->NewGlobalRef(domAccessorClass);
    if (!domAccessorClass) return PR_FALSE;

    startURLLoadMID = 
	env->GetStaticMethodID(domAccessorClass,
			       "startURLLoad",
			       "(Ljava/lang/String;Ljava/lang/String;J)V");
    if (Cleanup(env) == PR_TRUE) {
	domAccessorClass = NULL;
	return PR_FALSE;
    }
    
    endURLLoadMID = 
	env->GetStaticMethodID(domAccessorClass,
			       "endURLLoad",
			       "(Ljava/lang/String;IJ)V");
    if (Cleanup(env) == PR_TRUE) {
      domAccessorClass = NULL;
      return PR_FALSE;
    }

    progressURLLoadMID = 
	env->GetStaticMethodID(domAccessorClass,
			       "progressURLLoad",
			       "(Ljava/lang/String;IIJ)V");
    if (Cleanup(env) == PR_TRUE) {
	domAccessorClass = NULL;
      return PR_FALSE;
    }

    statusURLLoadMID = 
	env->GetStaticMethodID(domAccessorClass,
			       "statusURLLoad",
			       "(Ljava/lang/String;Ljava/lang/String;J)V");
    if (Cleanup(env) == PR_TRUE) {
	domAccessorClass = NULL;
	return PR_FALSE;
    }
    
    startDocumentLoadMID = 
	env->GetStaticMethodID(domAccessorClass,
			   "startDocumentLoad",
			       "(Ljava/lang/String;)V");
    if (Cleanup(env) == PR_TRUE) {
      domAccessorClass = NULL;
      return PR_FALSE;
    }
    
    endDocumentLoadMID = 
	env->GetStaticMethodID(domAccessorClass,
			       "endDocumentLoad",
			       "(Ljava/lang/String;IJ)V");
    if (Cleanup(env) == PR_TRUE) {
	domAccessorClass = NULL;
	return PR_FALSE;
    }
  }
  return PR_TRUE;
}

nsJavaDOMImpl::~nsJavaDOMImpl()
{
}

PRBool nsJavaDOMImpl::Cleanup(JNIEnv* env) 
{
  if (env->ExceptionOccurred()) {
    env->ExceptionDescribe();
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult nsJavaDOMImpl::GetDocument(nsIWebProgress* aWebProgress,
				    nsIDOMDocument **aResult)
{
  nsCOMPtr<nsIDOMWindow> domWin;
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsresult rv = NS_OK;

  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  
  if (nsnull != aWebProgress) {
    if (NS_SUCCEEDED(aWebProgress->GetDOMWindow(getter_AddRefs(domWin)))
	&& domWin) {
      if (NS_SUCCEEDED(rv = domWin->GetDocument(getter_AddRefs(domDoc)))) {
	*aResult = domDoc.get();
	return rv;
      }
    }
  }

  fprintf(stderr, 
	  "nsJavaDOMImpl::GetDocument: failed: "
	  "webProgress=%x, domWin=%x, domDoc=%x, "
	  "error=%d\n",
	  aWebProgress,
	  domWin.get(),
	  domDoc.get(),
	  rv);
  return rv;
}

//
// nsIWebProgressListener implementation
//

NS_IMETHODIMP nsJavaDOMImpl::OnStateChange(nsIWebProgress *aWebProgress, 
					   nsIRequest *aRequest, 
					   PRUint32 aStateFlags, 
					   PRUint32 aStatus)
{
  nsCAutoString name;
  nsresult rv = NS_OK;

  if (NS_FAILED(rv = aRequest->GetName(name))) {
    return rv;
  }
  nsAutoString uname;
  uname.AssignWithConversion(name.get());

  if ((aStateFlags & STATE_START) && (aStateFlags & STATE_IS_DOCUMENT)) {
    doStartDocumentLoad(uname.get());
  }
  if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_DOCUMENT)) {
    doEndDocumentLoad(aWebProgress, aRequest, aStatus);
  }
  if ((aStateFlags & STATE_START) && (aStateFlags & STATE_IS_REQUEST)) {
    doStartURLLoad(aWebProgress, aRequest);
  }
  if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_REQUEST)) {
    doEndURLLoad(aWebProgress, aRequest, aStatus);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsJavaDOMImpl::OnSecurityChange(nsIWebProgress *aWebProgress,
                                                  nsIRequest *aRequest, 
                                                  PRUint32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onProgressChange (in nsIChannel channel, in long curSelfProgress, in long maxSelfProgress, in long curTotalProgress, in long maxTotalProgress); */
NS_IMETHODIMP nsJavaDOMImpl::OnProgressChange(nsIWebProgress *aWebProgress, 
					      nsIRequest *request, 
					      PRInt32 aCurSelfProgress, 
					      PRInt32 aMaxSelfProgress, 
					      PRInt32 curTotalProgress, 
					      PRInt32 maxTotalProgress)
{
    nsCOMPtr<nsIDOMDocument> domDoc;
    nsresult rv;
    
    if (NS_FAILED(rv = GetDocument(aWebProgress, getter_AddRefs(domDoc)))) {
      return rv;
    }

    JNIEnv* env = NULL;
    if (Init(&env) == PR_FALSE) return NS_ERROR_FAILURE;

    nsCString urlSpecString;
    char* urlSpec = (char*) "";
    nsIURI* url = nsnull;
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if (channel && NS_SUCCEEDED(channel->GetURI(&url)) &&
	NS_SUCCEEDED(url->GetSpec(urlSpecString))) {
	urlSpec = ToNewCString(urlSpecString);
    }
    
    jstring jURL = env->NewStringUTF(urlSpec);
    nsMemory::Free(urlSpec);
    if (!jURL) return NS_ERROR_FAILURE;
    // PENDING(edburns): this leaks.
    
    env->CallStaticVoidMethod(domAccessorClass,
			      progressURLLoadMID,
			      jURL,
			      (jint) curTotalProgress,
			      (jint) maxTotalProgress,
			      (jlong)domDoc.get());
    if (Cleanup(env) == PR_TRUE) return NS_ERROR_FAILURE;
    return NS_OK;
}

/* void onStatusChange (in nsIChannel channel, in long progressStatusFlags); */
NS_IMETHODIMP nsJavaDOMImpl::OnStatusChange(nsIWebProgress *aWebProgress, 
                                                nsIRequest *request, 
                                                nsresult aStatus, 
                                                const PRUnichar *cMsg)
{
    nsCOMPtr<nsIDOMDocument> domDoc;
    nsresult rv;
    
    if (NS_FAILED(rv = GetDocument(aWebProgress, getter_AddRefs(domDoc)))) {
      return rv;
    }
    
    JNIEnv* env = NULL;
    if (Init(&env) == PR_FALSE) return NS_ERROR_FAILURE;
    
    char* urlSpec = (char*) "";
    nsCString urlSpecString;
    nsIURI* url = nsnull; 
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if (channel && NS_SUCCEEDED(channel->GetURI(&url)) &&
	NS_SUCCEEDED(url->GetSpec(urlSpecString))) {
	urlSpec = ToNewCString(urlSpecString);
    }
    jstring jURL = env->NewStringUTF(urlSpec);
    nsMemory::Free(urlSpec);
    if (!jURL) return NS_ERROR_FAILURE;
    // PENDING(edburns): this leaks

    jstring jMessage = env->NewString((jchar*) cMsg, nsCRT::strlen(cMsg));
    if (!jMessage) return NS_ERROR_FAILURE;
    
    env->CallStaticVoidMethod(domAccessorClass,
			      statusURLLoadMID,
			      jURL,
			      jMessage,
			      (jlong)domDoc.get());
    if (Cleanup(env) == PR_TRUE) return NS_ERROR_FAILURE;
    return NS_OK;
}

/* void onLocationChange (in nsIURI location); */
NS_IMETHODIMP nsJavaDOMImpl::OnLocationChange(nsIWebProgress *aWebProgress, 
                                                  nsIRequest *aRequest, 
                                                  nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//
// helper methods for nsIWebProgressListener 
//

NS_IMETHODIMP nsJavaDOMImpl::doStartDocumentLoad(const PRUnichar *documentName)
{
  JNIEnv* env = NULL;
  if (Init(&env) == PR_FALSE) return NS_ERROR_FAILURE;
  
  if (!documentName) {
    return NS_ERROR_FAILURE;
  }
  jstring jURL = env->NewString((jchar*) documentName, nsCRT::strlen(documentName));
  if (!jURL) return NS_ERROR_FAILURE;
  
  env->CallStaticVoidMethod(domAccessorClass,
			    startDocumentLoadMID,
			    jURL);
  // PENDING(edburns): this leaks jURL.  A bug?
  if (Cleanup(env) == PR_TRUE) return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP nsJavaDOMImpl::doEndDocumentLoad(nsIWebProgress *aWebProgress,
					       nsIRequest *request, 
					       PRUint32 aStatus)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsresult rv;
  
  if (NS_FAILED(rv = GetDocument(aWebProgress, getter_AddRefs(domDoc)))) {
    return rv;
  }

  JNIEnv* env = NULL;
  if (Init(&env) == PR_FALSE) return NS_ERROR_FAILURE;
  
  char* urlSpec = (char*) "";
  nsCString urlSpecString;
  nsIURI* url = nsnull;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (channel && NS_SUCCEEDED(channel->GetURI(&url)) &&
      NS_SUCCEEDED(url->GetSpec(urlSpecString))) {
	urlSpec = ToNewCString(urlSpecString);
  }

  jstring jURL = env->NewStringUTF(urlSpec);
  nsMemory::Free(urlSpec);
  if (!jURL) return NS_ERROR_FAILURE;
  
  env->CallStaticVoidMethod(domAccessorClass,
			    endDocumentLoadMID,
			    jURL,
			    (jint) aStatus,
			    (jlong)domDoc.get());
  if (Cleanup(env) == PR_TRUE) return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP nsJavaDOMImpl::doStartURLLoad(nsIWebProgress *aWebProgress, 
					    nsIRequest *request)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsresult rv;
  
  if (NS_FAILED(rv = GetDocument(aWebProgress, getter_AddRefs(domDoc)))) {
    return rv;
  }

  JNIEnv* env = NULL;
  if (Init(&env) == PR_FALSE) return NS_ERROR_FAILURE;

  char* urlSpec = (char*) "";
  nsCString urlSpecString;
  nsIURI* url = nsnull;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (channel && NS_SUCCEEDED(channel->GetURI(&url)) &&
      NS_SUCCEEDED(url->GetSpec(urlSpecString))) {
	urlSpec = ToNewCString(urlSpecString);
  }
  
  jstring jURL = env->NewStringUTF(urlSpec);
  nsMemory::Free(urlSpec);
  if (!jURL) return NS_ERROR_FAILURE;

  nsCAutoString contentType("");
  if (channel)
      channel->GetContentType(contentType);
  if (!contentType.Length()) 
    contentType = (char*) "";
  jstring jContentType = env->NewStringUTF(contentType.get());
  if (!jContentType) return NS_ERROR_FAILURE;

  env->CallStaticVoidMethod(domAccessorClass,
			    startURLLoadMID,
			    jURL,
			    jContentType,
			    (jlong)domDoc.get());
  if (Cleanup(env) == PR_TRUE) return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP nsJavaDOMImpl::doEndURLLoad(nsIWebProgress *aWebProgress, 
					  nsIRequest *request, 
					  PRUint32 aStatus)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsresult rv;
  
  if (NS_FAILED(rv = GetDocument(aWebProgress, getter_AddRefs(domDoc)))) {
    return rv;
  }

  JNIEnv* env = NULL;
  if (Init(&env) == PR_FALSE) return NS_ERROR_FAILURE;

    nsCString urlSpecString;
  char* urlSpec = (char*) "";
  nsIURI* url = nsnull;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (channel && NS_SUCCEEDED(channel->GetURI(&url)) &&
      NS_SUCCEEDED(url->GetSpec(urlSpecString))) {
      urlSpec = ToNewCString(urlSpecString);
  }
  
  jstring jURL = env->NewStringUTF(urlSpec);
  nsMemory::Free(urlSpec);
  if (!jURL) return NS_ERROR_FAILURE;

#if defined(DEBUG)
  dump_document(domDoc, urlSpec);
#endif
  env->CallStaticVoidMethod(domAccessorClass,
			    endURLLoadMID,
			    jURL,
			    (jint) aStatus,
			    (jlong)domDoc.get());
  if (Cleanup(env) == PR_TRUE) return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP nsJavaDOMImpl::HandleUnknownContentType(nsIDocumentLoader* loader,
						      nsIChannel* channel, 
						      const char *aContentType,
						      const char *aCommand)
{
  return NS_OK;
}

JNIEnv* nsJavaDOMImpl::GetJNIEnv() {
   JNIEnv* env;
#ifdef JAVA_DOM_OJI_ENABLE
   nsresult result;
   if (!jvmManager) {
       NS_WITH_SERVICE(nsIJVMManager, _jvmManager, kJVMManagerCID, &result);
       if (NS_SUCCEEDED(result)) {
           jvmManager = (nsJVMManager*)((nsIJVMManager*)_jvmManager);
       }
   }
   if (!jvmManager) {
       return NULL;
   }
   jvmManager->CreateProxyJNI(NULL,&env);
//     if (!securityContext) {
//         securityContext = new JavaDOMSecurityContext();
//     }
//     SetSecurityContext(env, securityContext);
   SetSecurityContext(env, new JavaDOMSecurityContext());
#else  /* JAVA_DOM_OJI_ENABLE */
   if (!jvm) {
	StartJVM();
   }
   jvm->AttachCurrentThread(&env,NULL);
#endif /* JAVA_DOM_OJI_ENABLE */
   return env;
}

#ifndef JAVA_DOM_OJI_ENABLE
void nsJavaDOMImpl::StartJVM(void) {
}
#endif /* JAVA_DOM_OJI_ENABLE */

#if defined(DEBUG)
static void dump_document(nsIDOMDocument* domDoc, const char* urlSpec)
{
  if (!domDoc) return;

  if (strstr(urlSpec, ".xul") ||
      strstr(urlSpec, ".js") ||
      strstr(urlSpec, ".css") ||
      strstr(urlSpec, "file:")) return;

  FILE* of = fopen("dom-cpp.txt", "a");
  if (!of) {
    perror("nsJavaDOMImpl::dump_document: failed to open output file\n");
    return;
  }

  nsIDOMElement* element = nsnull;
  nsresult rv = domDoc->GetDocumentElement(&element);
  if (NS_FAILED(rv) || !element)
    return;

  (void) element->Normalize();

  fprintf(of, "\n+++ %s +++\n", urlSpec);
  dump_node(of, (nsIDOMNode*) element, 0, PR_FALSE);
  fprintf(of, "\n");
  fflush(of);
  fclose(of);
}

static void dump_node(FILE* of, nsIDOMNode* node, int indent, 
		      PRBool isMapNode)
{
  if (!node) return;

  fprintf(of, "\n");
  for (int i=0; i < indent; i++)
    fprintf(of, " ");

  nsString name;
  nsString value;
  PRUint16 type;

  node->GetNodeName(name);
  node->GetNodeValue(value);
  node->GetNodeType(&type);

  const PRUnichar* cname = name.get();
  const PRUnichar* cvalue = value.get();
  char* cnorm = strip_whitespace(cvalue, value.Length());
  fprintf(of, "name=\"%s\" type=%s value=\"%s\"", 
	  cname, describe_type(type), cnorm);
  delete[] cnorm;

  if (isMapNode) return;

  nsIDOMNamedNodeMap* map = nsnull;
  node->GetAttributes(&map);
  dump_map(of, map, indent);

  nsIDOMNodeList* children = nsnull;
  node->GetChildNodes(&children);
  if (!children) return;

  PRUint32 length=0;
  children->GetLength(&length);
  for (PRUint32 j=0; j < length; j++) {
    nsIDOMNode* child = nsnull;
    children->Item(j, &child);
    dump_node(of, child, indent+2, PR_FALSE);
  }
}

static void dump_map(FILE* of, nsIDOMNamedNodeMap* map, int indent)
{
  if (!map) return;

  fprintf(of, "\n");
  PRUint32 length=0;
  map->GetLength(&length);
  if (length > 0) {
    for (int i=0; i < indent; i++)
      fprintf(of, " ");
    fprintf(of, "------- start attributes -------");
  }

  for (PRUint32 j=0; j < length; j++) {
    nsIDOMNode* node = nsnull;
    map->Item(j, &node);
    dump_node(of, node, indent, PR_TRUE);
  }

  if (length > 0) {
    fprintf(of, "\n");
    for (int k=0; k < indent; k++)
      fprintf(of, " ");
    fprintf(of, "------- end attributes -------");
  }
}

static char* strip_whitespace(const PRUnichar* input, int length)
{
  if (!input || length < 1) {
    char* out = new char[1];
    out[0] = 0;
    return out;
  }

  char* out = new char[length+1];
  char* op = out;
  const PRUnichar* ip = input;
  PRUnichar c = ' ';
  PRUnichar pc = ' ';
  int i = 0;

  for (c = *ip++; i++<length; c = *ip++) {
    if ((pc == ' ' || pc == '\n' || pc == '\t') &&
	(c == ' ' || c == '\n' || c == '\t'))
      continue;
    *op++ = (char)c;
    pc = c;
  }
  *op++ = 0;

  return out;
}

static const char* describe_type(int type)
{
  switch (type) {
  case nsIDOMNode::ELEMENT_NODE: return "ELEMENT";
  case nsIDOMNode::ATTRIBUTE_NODE: return "ATTRIBUTE";
  case nsIDOMNode::TEXT_NODE: return "TEXT";
  case nsIDOMNode::CDATA_SECTION_NODE: return "CDATA_SECTION";
  case nsIDOMNode::ENTITY_REFERENCE_NODE: return "ENTITY_REFERENCE";
  case nsIDOMNode::ENTITY_NODE: return "ENTITY";
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE: return "PROCESSING_INSTRUCTION";
  case nsIDOMNode::COMMENT_NODE: return "COMMENT";
  case nsIDOMNode::DOCUMENT_NODE: return "DOCUMENT";
  case nsIDOMNode::DOCUMENT_TYPE_NODE: return "DOCUMENT_TYPE";
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE: return "DOCUMENT_FRAGMENT";
  case nsIDOMNode::NOTATION_NODE: return "NOTATION";
  }
  return "ERROR";
}
#endif
