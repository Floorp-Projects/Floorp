/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* A namespace class for static layout utilities. */

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsLayoutUtils.h"

// static 
nsresult 
nsLayoutUtils::GetStaticScriptGlobal(JSContext* aContext,
                                     JSObject* aObj,
                                     nsIScriptGlobalObject** aNativeGlobal)
{
  nsISupports* supports;
  JSClass* clazz;
  JSObject* parent;
  JSObject* glob = aObj; // starting point for search

  if (!glob)
    return NS_ERROR_FAILURE;

  while (nsnull != (parent = JS_GetParent(aContext, glob)))
    glob = parent;

#ifdef JS_THREADSAFE
  clazz = JS_GetClass(aContext, glob);
#else
  clazz = JS_GetClass(glob);
#endif

  if (!clazz ||
      !(clazz->flags & JSCLASS_HAS_PRIVATE) ||
      !(clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) ||
      !(supports = (nsISupports*) JS_GetPrivate(aContext, glob))) {
    return NS_ERROR_FAILURE;
  }

  return supports->QueryInterface(NS_GET_IID(nsIScriptGlobalObject),
                                  (void**) aNativeGlobal);
}

//static
nsresult 
nsLayoutUtils::GetStaticScriptContext(JSContext* aContext,
                                      JSObject* aObj,
                                      nsIScriptContext** aScriptContext)
{
  nsCOMPtr<nsIScriptGlobalObject> nativeGlobal;
  GetStaticScriptGlobal(aContext, aObj, getter_AddRefs(nativeGlobal));
  if (!nativeGlobal)    
    return NS_ERROR_FAILURE;
  nsIScriptContext* scriptContext = nsnull;
  nativeGlobal->GetContext(&scriptContext);
  *aScriptContext = scriptContext;
  return scriptContext ? NS_OK : NS_ERROR_FAILURE;
}  

//static
nsresult 
nsLayoutUtils::GetDynamicScriptGlobal(JSContext* aContext,
                                      nsIScriptGlobalObject** aNativeGlobal)
{
  nsIScriptGlobalObject* nativeGlobal = nsnull;
  nsCOMPtr<nsIScriptContext> scriptCX;
  GetDynamicScriptContext(aContext, getter_AddRefs(scriptCX));
  if (scriptCX) {
    *aNativeGlobal = nativeGlobal = scriptCX->GetGlobalObject();
  }
  return nativeGlobal ? NS_OK : NS_ERROR_FAILURE;
}  

//static
nsresult 
nsLayoutUtils::GetDynamicScriptContext(JSContext *aContext,
                                       nsIScriptContext** aScriptContext)
{
  // XXX We rely on the rule that if any JSContext in our JSRuntime has a 
  // private set then that private *must* be a pointer to an nsISupports.
  nsISupports *supports = (nsIScriptContext*) JS_GetContextPrivate(aContext);
  if (!supports)
      return nsnull;
  return supports->QueryInterface(NS_GET_IID(nsIScriptContext),
                                  (void**)aScriptContext);
}

template <class OutputIterator> 
struct NormalizeNewlinesCharTraits {
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    NormalizeNewlinesCharTraits(OutputIterator& aIterator) : mIterator(aIterator) { }
    void writechar(typename OutputIterator::value_type aChar) {
      *mIterator++ = aChar;
    }

  private:
    OutputIterator mIterator;
};

#ifdef HAVE_CPP_PARTIAL_SPECIALIZATION

template <class CharT>
struct NormalizeNewlinesCharTraits<CharT*> {
  public:
    typedef CharT value_type;

  public: 
    NormalizeNewlinesCharTraits(CharT* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(CharT aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    CharT* mCharPtr;
}; 

#else

NS_SPECIALIZE_TEMPLATE
struct NormalizeNewlinesCharTraits<char*> {
  public:
    typedef char value_type;

  public: 
    NormalizeNewlinesCharTraits(char* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(char aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    char* mCharPtr;
};

NS_SPECIALIZE_TEMPLATE
struct NormalizeNewlinesCharTraits<PRUnichar*> {
  public:
    typedef PRUnichar value_type;

  public: 
    NormalizeNewlinesCharTraits(PRUnichar* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(PRUnichar aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    PRUnichar* mCharPtr;
};

#endif

template <class OutputIterator> 
class CopyNormalizeNewlines
{
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    CopyNormalizeNewlines(OutputIterator* aDestination) : 
      mLastCharCR(PR_FALSE),
      mDestination(aDestination),
      mWritten(0) 
    { }
    
    PRUint32 GetCharsWritten() { 
      return mWritten; 
    }

    PRUint32 write(const typename OutputIterator::value_type* aSource, PRUint32 aSourceLength) {
      // If the last source buffer ended with a CR...
      if (mLastCharCR) {
        // ..and if the next one is a LF, then skip it since
        // we've already written out a newline
        if (aSourceLength && (*aSource == value_type('\n'))) {
          aSource++;
        }
        mLastCharCR = PR_FALSE;
      }

      const typename OutputIterator::value_type* done_writing = aSource + aSourceLength;
      PRUint32 num_written = 0;
      while ( aSource < done_writing ) {
        if (*aSource == value_type('\r')) {
          mDestination->writechar('\n');
          aSource++;
          // If we've reached the end of the buffer, record
          // that we wrote out a CR
          if (aSource == done_writing) {
            mLastCharCR = PR_TRUE;
          }
          // If the next character is a LF, skip it
          else if (*aSource == value_type('\n')) {
            aSource++;
          }
        }
        else {
          mDestination->writechar(*aSource++);
        }
        num_written++;
      }
       
      mWritten += num_written;
      return aSourceLength;
    }

  private:
    PRBool mLastCharCR;
    OutputIterator* mDestination;
    PRUint32 mWritten;
};

// static
PRUint32 
nsLayoutUtils::CopyNewlineNormalizedUnicodeTo(const nsAReadableString& aSource, PRUint32 aSrcOffset, PRUnichar* aDest, PRUint32 aLength)
{
  typedef NormalizeNewlinesCharTraits<PRUnichar*> sink_traits;

  sink_traits dest_traits(aDest);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits);
  nsReadingIterator<PRUnichar> fromBegin, fromEnd;
  copy_string(aSource.BeginReading(fromBegin).advance( PRInt32(aSrcOffset) ), aSource.BeginReading(fromEnd).advance( PRInt32(aSrcOffset+aLength) ), normalizer);
  return normalizer.GetCharsWritten();
}

// static
PRUint32 
nsLayoutUtils::CopyNewlineNormalizedUnicodeTo(nsReadingIterator<PRUnichar>& aSrcStart, const nsReadingIterator<PRUnichar>& aSrcEnd, nsAWritableString& aDest)
{
  typedef nsWritingIterator<PRUnichar> WritingIterator;
  typedef NormalizeNewlinesCharTraits<WritingIterator> sink_traits;

  WritingIterator iter;
  aDest.BeginWriting(iter);
  sink_traits dest_traits(iter);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits);
  copy_string(aSrcStart, aSrcEnd, normalizer);
  return normalizer.GetCharsWritten();
}
