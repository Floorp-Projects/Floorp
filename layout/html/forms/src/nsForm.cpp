/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsHTMLForms.h"
#include "nsIFormManager.h"
#include "nsIFormControl.h"
#include "nsIAtom.h"
#include "nsHTMLIIDs.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "nsVoidArray.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsCRT.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsILinkHandler.h"
#include "nsInputRadio.h"
#include "nsIRadioButton.h"
#include "nsInputFile.h"
#include "nsDocument.h"

#include "net.h"
#include "xp_file.h"
#include "prio.h"
#include "prmem.h"
#include "prenv.h"

#define CRLF "\015\012"   

// netlib has a general function (netlib\modules\liburl\src\escape.c) 
// which does url encoding. Since netlib is not yet available for raptor,
// the following will suffice. Convert space to +, don't convert alphanumeric,
// conver each non alphanumeric char to %XY where XY is the hexadecimal 
// equavalent of the binary representation of the character. 
// 
void URLEncode(char* aInString, char* aOutString)
{
  if (nsnull == aInString) {
	return;
  }
  static char *toHex = "0123456789ABCDEF";
  char* outChar = aOutString;
  for (char* inChar = aInString; *inChar; inChar++) {
    if(' ' == *inChar) {                                     // convert space to +
	  *outChar++ = '+';
	} else if ( (((*inChar - '0') >= 0) && (('9' - *inChar) >= 0)) || // don't conver 
                (((*inChar - 'a') >= 0) && (('z' - *inChar) >= 0)) || // alphanumeric
				(((*inChar - 'A') >= 0) && (('Z' - *inChar) >= 0)) ) {
	  *outChar++ = *inChar;
	} else {                                                 // convert all else to hex
	  *outChar++ = '%';
      *outChar++ = toHex[(*inChar >> 4) & 0x0F];
      *outChar++ = toHex[*inChar & 0x0F];
	}
  }
  *outChar = 0;  // terminate the string
}

nsString* URLEncode(nsString& aString) 
{  
  char* inBuf  = aString.ToNewCString();
	char* outBuf = new char[ (strlen(inBuf) * 3) + 1 ];
	URLEncode(inBuf, outBuf);
	nsString* result = new nsString(outBuf);
	delete [] outBuf;
	delete [] inBuf;
	return result;
}


//----------------------------------------------------------------------

static NS_DEFINE_IID(kIFormManagerIID, NS_IFORMMANAGER_IID);

class nsForm : public nsIFormManager
{
public:
  // Construct a new Form Element with no attributes. This needs to be
  // made private and have a static COM create method.
  nsForm(nsIAtom* aTag);
  virtual ~nsForm();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual void OnRadioChecked(nsIFormControl& aRadio);
    
  // callback for reset button controls. 
  virtual void OnReset();

  // callback for text and textarea controls. If there is a single
  // text/textarea and a return is entered, then this is equavalent to
  // a submit.
  virtual void OnReturn();

  // callback for submit button controls.
  virtual void OnSubmit(nsIPresContext* aPresContext, nsIFrame* aFrame, 
                        nsIFormControl* aSubmitter);

  // callback for tabs on controls that can gain focus. This will
  // eventually need to be handled at the document level to support
  // the tabindex attribute.
  virtual void OnTab();

  virtual PRInt32 GetFormControlCount() const;
  virtual nsIFormControl* GetFormControlAt(PRInt32 aIndex) const;
  virtual PRBool AddFormControl(nsIFormControl* aFormControl);
  virtual PRBool RemoveFormControl(nsIFormControl* aFormControl, 
                                   PRBool aChildIsRef = PR_TRUE);

  virtual void SetAttribute(const nsString& aName, const nsString& aValue);

  virtual PRBool GetAttribute(const nsString& aName,
                              nsString& aResult) const;

  virtual nsresult GetRefCount() const;

  virtual void Init(PRBool aReinit);

  virtual nsFormRenderingMode GetMode() const { return mRenderingMode; }
  virtual void SetMode(nsFormRenderingMode aMode) { mRenderingMode = aMode; }

  static nsString* gGET;
  static nsString* gMULTIPART;


protected:
  void RemoveRadioGroups();
  void ProcessAsURLEncoded(PRBool aIsPost, nsIFormControl* aSubmitter, nsString& aData);
  void ProcessAsMultipart(nsIFormControl* aSubmitter, nsString& aData);
  static const char* GetFileNameWithinPath(char* aPathName);

  // the following are temporary until nspr and/or netlib provide them
  static Temp_GetTempDir(char* aTempDirName);
  static char* Temp_GenerateTempFileName(PRInt32 aMaxSize, char* aBuffer);
  static void Temp_GetContentType(char* aPathName, char* aContentType);

  nsIAtom* mTag;
  nsIHTMLAttributes* mAttributes;
  nsVoidArray mChildren;
  nsVoidArray mRadioGroups;
  nsString* mAction;
  nsString* mEncoding;
  nsString* mTarget;
  PRInt32 mMethod;
  PRBool  mInited;
  nsFormRenderingMode mRenderingMode;
};

#define METHOD_UNSET    0
#define METHOD_GET      1
#define METHOD_POST     2

// CLASS nsForm

nsString* nsForm::gGET       = new nsString("get");
nsString* nsForm::gMULTIPART = new nsString("multipart/form-data");

// Note: operator new zeros our memory
nsForm::nsForm(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mTag = aTag;
  NS_IF_ADDREF(aTag);
  mInited = PR_FALSE;
}

nsForm::~nsForm()
{
  NS_IF_RELEASE(mTag);
  int numChildren = GetFormControlCount();
  for (int i = 0; i < numChildren; i++) {
    nsIFormControl* child = GetFormControlAt(i);
    RemoveFormControl(child, PR_FALSE);
    child->SetFormManager(nsnull, PR_FALSE);
    NS_RELEASE(child);
  }
  if (nsnull != mAction) delete mAction;
  if (nsnull != mEncoding) delete mEncoding;
  if (nsnull != mTarget) delete mTarget;

  RemoveRadioGroups();
}

NS_IMPL_QUERY_INTERFACE(nsForm,kIFormManagerIID);
NS_IMPL_ADDREF(nsForm);

nsrefcnt nsForm::GetRefCount() const
{
  return mRefCnt;
}

nsrefcnt nsForm::Release()
{
  --mRefCnt;
  int numChildren = GetFormControlCount();
  PRBool externalRefsToChildren = PR_FALSE;  // are there refs to any children besides my ref
  for (int i = 0; i < numChildren; i++) {
    nsIFormControl* child = GetFormControlAt(i);
    if (child->GetRefCount() > 1) {
      externalRefsToChildren = PR_TRUE;
      break;
    }
  }
  if (!externalRefsToChildren && ((int)mRefCnt == numChildren)) {
    mRefCnt = 0;
    delete this; 
    return 0;
  } 
  return mRefCnt;
}

PRInt32 
nsForm::GetFormControlCount() const 
{ 
  return mChildren.Count(); 
}

nsIFormControl* 
nsForm::GetFormControlAt(PRInt32 aIndex) const 
{ 
  nsIFormControl* ctl = (nsIFormControl*) mChildren.ElementAt(aIndex);
  NS_IF_ADDREF(ctl);
  return ctl;
}

PRBool 
nsForm::AddFormControl(nsIFormControl* aChild) 
{ 
  PRBool rv = mChildren.AppendElement(aChild);
  if (rv) {
    NS_ADDREF(aChild);
  }
  return rv;
}

PRBool 
nsForm::RemoveFormControl(nsIFormControl* aChild, PRBool aChildIsRef) 
{ 
  PRBool rv = mChildren.RemoveElement(aChild);
  if (rv && aChildIsRef) {
    NS_RELEASE(aChild);
  }
  return rv;
}

void 
nsForm::OnReset()
{
  PRInt32 numChildren = mChildren.Count();
  for (int childX = 0; childX < numChildren; childX++) {
	  nsIFormControl* child = (nsIFormControl*) mChildren.ElementAt(childX);
	  child->Reset();
	}
}

void 
nsForm::OnReturn()
{
}

void DebugPrint(char* aLabel, nsString aString)
{
  char* out = aString.ToNewCString();
  printf("\n %s=%s\n", aLabel, out);
  delete [] out;
}

void nsForm::OnSubmit(nsIPresContext* aPresContext, nsIFrame* aFrame, 
                      nsIFormControl* aSubmitter)
{
  nsString data; // this could be more efficient, by allocating a larger buffer

  nsAutoString method, enctype;
  GetAttribute("method", method);
  GetAttribute("enctype", enctype);

  PRBool isURLEncoded = (enctype.EqualsIgnoreCase(*gMULTIPART)) ? PR_FALSE : PR_TRUE;

  // for enctype=multipart/form-data, force it to be post
  PRBool isPost = (method.EqualsIgnoreCase(*gGET) && isURLEncoded) ? PR_FALSE : PR_TRUE;

  if (isURLEncoded) {
    ProcessAsURLEncoded(isPost, aSubmitter, data);
  }
  else {
    ProcessAsMultipart(aSubmitter, data);
  }


  // make the url string
  nsILinkHandler* handler;
  if (NS_OK == aPresContext->GetLinkHandler(&handler)) {
    // Resolve url to an absolute url
    nsIURL* docURL = nsnull;
    nsIContent* content;
    aFrame->GetContent(content);
    if (nsnull != content) {
      nsIDocument* doc = nsnull;
      content->GetDocument(doc);
      if (nsnull != doc) {
        docURL = doc->GetDocumentURL();
        NS_RELEASE(doc);
      }
      NS_RELEASE(content);
    }

    nsAutoString target;
    GetAttribute("target", target);

    nsAutoString href;
    GetAttribute("action", href);
    if (!isPost) {
      href.Append(data);
    }
    nsAutoString absURLSpec;
    nsAutoString base;
    nsresult rv = NS_MakeAbsoluteURL(docURL, base, href, absURLSpec);
    NS_IF_RELEASE(docURL);

    // Now pass on absolute url to the click handler
    nsIPostData* postData = nsnull;
    if (isPost) {
      nsresult rv;
      char* postBuffer = data.ToNewCString();

      rv = NS_NewPostData(!isURLEncoded, postBuffer, &postData);
      if (NS_OK != rv) {
        delete [] postBuffer;
      }

      /* The postBuffer is now owned by the IPostData instance */
    }
    handler->OnLinkClick(aFrame, absURLSpec, target, postData);
    NS_IF_RELEASE(postData);

DebugPrint("url", absURLSpec);
DebugPrint("data", data);
  }
}

void nsForm::ProcessAsURLEncoded(PRBool isPost, nsIFormControl* aSubmitter, nsString& aData)
{
  nsString buf;
  PRBool firstTime = PR_TRUE;

  PRInt32 numChildren = mChildren.Count();
  // collect and encode the data from the children controls
  for (PRInt32 childX = 0; childX < numChildren; childX++) {
	  nsIFormControl* child = (nsIFormControl*) mChildren.ElementAt(childX);
		if (child->IsSuccessful(aSubmitter)) {
		  PRInt32 numValues = 0;
		  PRInt32 maxNumValues = child->GetMaxNumValues();
			if (maxNumValues <= 0) {
				continue;
			}
		  nsString* names = new nsString[maxNumValues];
		  nsString* values = new nsString[maxNumValues];
			if (PR_TRUE == child->GetNamesValues(maxNumValues, numValues, values, names)) {
				for (int valueX = 0; valueX < numValues; valueX++) {
				  if (PR_TRUE == firstTime) {
					  firstTime = PR_FALSE;
				  } else {
				    buf += "&";
				  }
					nsString* convName = URLEncode(names[valueX]);
				  buf += *convName;
					delete convName;
					buf += "=";
					nsString* convValue = URLEncode(values[valueX]);
					buf += *convValue;
					delete convValue;
				}
			}
      delete [] names;
			delete [] values;
		}
	}

  aData.SetLength(0);
  if (isPost) {
    char size[16];
    sprintf(size, "%d", buf.Length());
    aData = "Content-type: application/x-www-form-urlencoded";
    aData += CRLF;
    aData += "Content-Length: ";
    aData += size;
    aData += CRLF;
    aData += CRLF;
  }
  else {
    aData += '?';
  }

  aData += buf;
}

// include the file name without the directory
const char* nsForm::GetFileNameWithinPath(char* aPathName)
{
  char* fileNameStart = PL_strrchr(aPathName, '\\'); // windows
  if (!fileNameStart) { // try unix
    fileNameStart = PL_strrchr(aPathName, '\\');
  }
  if (fileNameStart) { 
    return fileNameStart+1;
  }
  else {
    return aPathName;
  }
}

// this needs to be provided by a higher level, since navigator might override
// the temp directory. XXX does not check that parm is big enough
nsForm::Temp_GetTempDir(char* aTempDirName)
{
  aTempDirName[0] = 0;
  const char* env;

  if ((env = (const char *) getenv("TMP")) == nsnull) {
    if ((env = (const char *) getenv("TEMP")) == nsnull) {
	    strcpy(aTempDirName, ".");
    }
  }
  if (*env == '\0')	{	// null string means "." 
    strcpy(aTempDirName, ".");
  }
  if (0 == aTempDirName[0]) {
    strcpy(aTempDirName, env);
  }
  return PR_TRUE;
}

// the following is a temporary measure until NET_cinfo_find_type or its
// replacement is available
void nsForm::Temp_GetContentType(char* aPathName, char* aContentType)
{
  if (!aPathName) {
    strcpy(aContentType, "unknown");
    return;
  }

  int len = strlen(aPathName);
  if (len <= 0) {
    strcpy(aContentType, "unknown");
    return;
  }

  char* fileExt = &aPathName[len-1];
  for (int i = len-1; i >= 0; i--) {
    if ('.' == aPathName[i]) {
      break;
    }
    fileExt--;
  }
  if ((0 == nsCRT::strcasecmp(fileExt, ".html")) ||
      (0 == nsCRT::strcasecmp(fileExt, ".htm"))) {
    strcpy(aContentType, "text/html");
  }
  else if (0 == nsCRT::strcasecmp(fileExt, ".txt")) { 
    strcpy(aContentType, "text/plain");
  }
  else if (0 == nsCRT::strcasecmp(fileExt, ".gif")) { 
    strcpy(aContentType, "image/gif");
  }
  else if ((0 == nsCRT::strcasecmp(fileExt, ".jpeg")) ||
           (0 == nsCRT::strcasecmp(fileExt, ".jpg"))) {
    strcpy(aContentType, "image/jpeg");
  }
  else { // don't bother trying to do the others here
    strcpy(aContentType, "unknown");
  }
}


#if 0
  char* tmpDir = aFileName;		// copy name to fname */
  // Keep generating file names till we find one that's not in use
    while ((*env != '\0') && count++ {
      *ptr++ = *env++;
    }
    if (ptr[-1] != '\\' && ptr[-1] != '/')
      *ptr++ = '\\';		/* append backslash if not in env variable */
    /* Append a suitable file name */
    next_file_num++;		/* advance counter */
    sprintf(ptr, "JPG%03d.TMP", next_file_num);
    /* Probe to see if file name is already in use */
    if ((tfile = fopen(fname, READ_BINARY)) == NULL)
      break;
    fclose(tfile);		/* oops, it's there; close tfile & try again */
  }

  char* tempDir = getenv("temp");
  if (!tempDir) {
    tempDir = getenv("tmp");
  }
  if (tempDir) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
#endif

#define CONTENT_DISP "Content-Disposition: form-data; name=\""
#define FILENAME "\"; filename=\""
#define CONTENT_TYPE "Content-Type: "
#define CONTENT_ENCODING "Content-Encoding: "
#define BUFSIZE 1024
#define MULTIPART "multipart/form-data"
#define END "--"

void nsForm::ProcessAsMultipart(nsIFormControl* aSubmitter, nsString& aData)
{
  aData.SetLength(0);
  char buffer[BUFSIZE];
  PRInt32 numChildren = mChildren.Count();

  // construct a temporary file to put the data into 
  char tmpFileName[BUFSIZE];
  char* result = Temp_GenerateTempFileName((PRInt32)BUFSIZE, tmpFileName);
  if (!result) {
    return;
  }

  PRFileDesc* tmpFile = PR_Open(tmpFileName, PR_CREATE_FILE | PR_WRONLY, 0644);
  if (!tmpFile) {
    return;
	}

  // write the content-type, boundary to the tmp file
	char boundary[80];
  sprintf(boundary, "-----------------------------%d%d%d", 
          boundary, rand(), rand(), rand());
  sprintf(buffer, "Content-type: %s; boundary=%s" CRLF, MULTIPART, boundary);
	PRInt32 len = PR_Write(tmpFile, buffer, PL_strlen(buffer));
  if (len < 0) {
    return;
  }

  PRInt32 boundaryLen = PL_strlen(boundary);
	PRInt32	contDispLen = PL_strlen(CONTENT_DISP);
  PRInt32 crlfLen     = PL_strlen(CRLF);

  // compute the content length, passing through all of the form controls
	PRInt32 contentLen = crlfLen; // extra crlf after content-length header

  PRInt32 childX;  // stupid compiler
  for (childX = 0; childX < numChildren; childX++) {
	  nsIFormControl* child = (nsIFormControl*) mChildren.ElementAt(childX);
    nsAutoString type;
    child->GetType(type);
    if (child->IsSuccessful(aSubmitter)) {
		  PRInt32 numValues = 0;
		  PRInt32 maxNumValues = child->GetMaxNumValues();
			if (maxNumValues <= 0) {
        continue;
      }
		  nsString* names  = new nsString[maxNumValues];
		  nsString* values = new nsString[maxNumValues];
			if (PR_FALSE == child->GetNamesValues(maxNumValues, numValues, values, names)) {
        continue;
      }
      contentLen += boundaryLen + crlfLen;
      contentLen += contDispLen;
		  for (int valueX = 0; valueX < numValues; valueX++) {
        char* name  = names[valueX].ToNewCString();
        char* value = values[valueX].ToNewCString();
        if ((0 == names[valueX].Length()) || (0 == values[valueX].Length())) {
          continue;
        }
				contentLen += PL_strlen(name);
			  contentLen += 1 + crlfLen;  // ending name quote plus CRLF
        if (type.EqualsIgnoreCase(*nsInputFile::gFILE_TYPE)) { // <input type=file>
          contentLen += PL_strlen(FILENAME);

          // include the file name without the directory
          char* fileNameStart = PL_strrchr(value, '/'); // unix
          if (!fileNameStart) { // try windows
            fileNameStart = PL_strrchr(value, '\\');
          }
          fileNameStart = (fileNameStart) ? fileNameStart+1 : value; 
			    contentLen += PL_strlen(fileNameStart);

					// determine the content-type of the file

          char contentType[128];
					Temp_GetContentType(value, &contentType[0]);
				  contentLen += PL_strlen(CONTENT_TYPE);
					contentLen += PL_strlen(contentType) + crlfLen + crlfLen;

			    // get the size of the file
				  PRFileInfo fileInfo;
				  if (PR_SUCCESS == PR_GetFileInfo(value, &fileInfo)) {
					  contentLen += fileInfo.size;
          }
				}
        else {
          contentLen += PL_strlen(value) + crlfLen;
        }
        delete [] name;
        delete [] value;
      }
 			delete [] names;
			delete [] values;
		}

    aData = tmpFileName;
	}

  contentLen += boundaryLen + PL_strlen(END) + crlfLen;

  // write the content 
  sprintf(buffer, "Content-Length: %ld" CRLF CRLF, contentLen);
	PR_Write(tmpFile, buffer, PL_strlen(buffer));

  // write the content passing through all of the form controls a 2nd time
  for (childX = 0; childX < numChildren; childX++) {
	  nsIFormControl* child = (nsIFormControl*) mChildren.ElementAt(childX);
    nsAutoString type;
    child->GetType(type);
    if (child->IsSuccessful(aSubmitter)) {
		  PRInt32 numValues = 0;
		  PRInt32 maxNumValues = child->GetMaxNumValues();
			if (maxNumValues <= 0) {
        continue;
      }
		  nsString* names  = new nsString[maxNumValues];
		  nsString* values = new nsString[maxNumValues];
			if (PR_FALSE == child->GetNamesValues(maxNumValues, numValues, values, names)) {
        continue;
      }
		  for (int valueX = 0; valueX < numValues; valueX++) {
        char* name  = names[valueX].ToNewCString();
        char* value = values[valueX].ToNewCString();
        if ((0 == names[valueX].Length()) || (0 == values[valueX].Length())) {
          continue;
        }
	      sprintf(buffer, "%s" CRLF, boundary);
		    PR_Write(tmpFile, buffer, PL_strlen(buffer));
			  PR_Write(tmpFile, CONTENT_DISP, contDispLen);
				PR_Write(tmpFile, name, PL_strlen(name));

        if (type.EqualsIgnoreCase(*nsInputFile::gFILE_TYPE)) { // <input type=file>
				  PR_Write(tmpFile, FILENAME, PL_strlen(FILENAME));
          const char* fileNameStart = GetFileNameWithinPath(value);
          PR_Write(tmpFile, fileNameStart, PL_strlen(fileNameStart)); 
        }
			  PR_Write(tmpFile, "\"" CRLF, PL_strlen("\"" CRLF)); // end content disp

        if (type.EqualsIgnoreCase(*nsInputFile::gFILE_TYPE)) { 
					// determine the content-type of the file
          char contentType[128];
					Temp_GetContentType(value, &contentType[0]);
	        PR_Write(tmpFile, CONTENT_TYPE, PL_strlen(CONTENT_TYPE));
	        PR_Write(tmpFile, contentType, PL_strlen(contentType));
				  PR_Write(tmpFile, CRLF, PL_strlen(CRLF));
			    PR_Write(tmpFile, CRLF, PL_strlen(CRLF)); // end content-type header

          PRFileDesc* contentFile = PR_Open(value, PR_RDONLY, 0644);
					if(contentFile) {
            PRInt32 size;
						while((size = PR_Read(contentFile, buffer, BUFSIZE)) > 0) {
							PR_Write(tmpFile, buffer, size);
						}
						PR_Close(contentFile);
					}
				}
        else {
 					PR_Write(tmpFile, value, PL_strlen(value));
          PR_Write(tmpFile, CRLF, crlfLen);
        }
        delete [] name;
        delete [] value;
      }
 			delete [] names;
			delete [] values;
		}
	}

	sprintf(buffer, "%s--" CRLF, boundary);
  PR_Write(tmpFile, buffer, PL_strlen(buffer));

  PR_Close(tmpFile);
	
	//StrAllocCopy(url_struct->post_data, tmpfilename);
	//url_struct->post_data_is_file = TRUE;
}

void 
nsForm::OnTab()
{
}

void nsForm::SetAttribute(const nsString& aName, const nsString& aValue)
{
  nsAutoString tmp(aName);
  tmp.ToUpperCase();
  nsIAtom* atom = NS_NewAtom(tmp);

  if (atom == nsHTMLAtoms::action) {
    nsAutoString url(aValue);
    url.StripWhitespace();
    if (nsnull == mAction) {
      mAction = new nsString(url);
    }
    else {
      *mAction = url;
    }
  }
  else if (atom == nsHTMLAtoms::encoding) {
    if (nsnull == mEncoding) {
      mEncoding = new nsString(aValue);
    }
    else {
      *mEncoding = aValue;
    }
  }
  else if (atom == nsHTMLAtoms::target) {
    if (nsnull == mTarget) {
      mTarget = new nsString(aValue);
    }
    else {
      *mTarget = aValue;
    }
  }
  else if (atom == nsHTMLAtoms::method) {
    if (aValue.EqualsIgnoreCase("post")) {
      mMethod = METHOD_POST;
    }
    else {
      mMethod = METHOD_GET;
    }
  }
  // temporarily, use type attribute to set the rendering mode
  else if (atom == nsHTMLAtoms::type) {
    mRenderingMode = (aValue.EqualsIgnoreCase("forward")) ? kForwardMode : kBackwardMode;
  }
  else {
    // Use default storage for unknown attributes
    if (nsnull == mAttributes) {
      NS_NewHTMLAttributes(&mAttributes, nsnull);
    }
    if (nsnull != mAttributes) {
      mAttributes->SetAttribute(atom, aValue);
    }
  }
  NS_RELEASE(atom);
}

PRBool nsForm::GetAttribute(const nsString& aName,
                            nsString& aResult) const
{
  nsAutoString tmp(aName);
  tmp.ToUpperCase();
  nsIAtom* atom = NS_NewAtom(tmp);
  PRBool rv = PR_FALSE;
  if (atom == nsHTMLAtoms::action) {
    if (nsnull != mAction) {
      aResult = *mAction;
      rv = PR_TRUE;
    }
  }
  else if (atom == nsHTMLAtoms::encoding) {
    if (nsnull != mEncoding) {
      aResult = *mEncoding;
      rv = PR_TRUE;
    }
  }
  else if (atom == nsHTMLAtoms::target) {
    if (nsnull != mTarget) {
      aResult = *mTarget;
      rv = PR_TRUE;
    }
  }
  else if (atom == nsHTMLAtoms::method) {
    if (METHOD_UNSET != mMethod) {
      if (METHOD_POST == mMethod) {
        aResult = "post";
      }
      else {
        aResult = "get";
      }
      rv = PR_TRUE;
    }
  }
  else {
    // Use default storage for unknown attributes
    if (nsnull != mAttributes) {
      nsHTMLValue value;
      if (eContentAttr_HasValue == mAttributes->GetAttribute(atom, value)) {
        if (value.GetUnit() == eHTMLUnit_String) {
          value.GetStringValue(aResult);
          rv = PR_TRUE;
        }
      }
    }
  }

  NS_RELEASE(atom);
  return rv;
}

void nsForm::RemoveRadioGroups()
{
  int numRadioGroups = mRadioGroups.Count();
  for (int i = 0; i < numRadioGroups; i++) {
    nsInputRadioGroup* radioGroup = (nsInputRadioGroup *) mRadioGroups.ElementAt(i);
    delete radioGroup;
  }
  mRadioGroups.Clear();
}

void nsForm::Init(PRBool aReinit)
{
  if (mInited && !aReinit) {
    return;
  }
  mInited = PR_TRUE;
  RemoveRadioGroups();

  // determine which radio buttons belong to which radio groups, unnamed radio buttons
  // don't go into any group since they can't be submitted
  int numControls = GetFormControlCount();
  for (int i = 0; i < numControls; i++) {
    nsIFormControl* control = (nsIFormControl *)GetFormControlAt(i);
    nsString name;
    PRBool hasName = control->GetName(name);
    nsString type;
    control->GetType(type);
    if (hasName && (type.Equals(*nsInputRadio::kTYPE))) {
      int numGroups = mRadioGroups.Count();
      PRBool added = PR_FALSE;
      nsInputRadioGroup* group;
      for (int j = 0; j < numGroups; j++) {
        group = (nsInputRadioGroup *) mRadioGroups.ElementAt(j);
        nsString groupName;
        group->GetName(groupName);
        if (groupName.Equals(name)) {
          group->AddRadio(control);
          added = PR_TRUE;
          break;
        }
      }
      if (!added) {
        group = new nsInputRadioGroup(name);
        mRadioGroups.AppendElement(group);
        group->AddRadio(control);
      }
      // allow only one checked radio button
      if (control->GetChecked(PR_TRUE)) {
	      if (nsnull == group->GetCheckedRadio()) {
	        group->SetCheckedRadio(control);
	      }
	      else {
	        control->SetChecked(PR_FALSE, PR_TRUE);
	      }
      }
    }
  }
}
  
void
nsForm::OnRadioChecked(nsIFormControl& aControl)
{
  nsString radioName;
  if (!aControl.GetName(radioName)) { // don't consider a radio without a name 
    return;
  }
 
  // locate the radio group with the name of aRadio
  int numGroups = mRadioGroups.Count();
  for (int j = 0; j < numGroups; j++) {
    nsInputRadioGroup* group = (nsInputRadioGroup *) mRadioGroups.ElementAt(j);
    nsString groupName;
    group->GetName(groupName);
    nsIFormControl* checkedRadio = group->GetCheckedRadio();
    if (groupName.Equals(radioName) && (nsnull != checkedRadio) & (&aControl != checkedRadio)) {
      checkedRadio->SetChecked(PR_FALSE, PR_FALSE);
      group->SetCheckedRadio(&aControl);
    }
  }
}

nsresult
NS_NewHTMLForm(nsIFormManager** aInstancePtrResult,
               nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsForm* it = new nsForm(aTag);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult result = it->QueryInterface(kIFormManagerIID, (void**) aInstancePtrResult);
  return result;
}

// THE FOLLOWING WAS TAKEN FROM CMD/WINFE/FEGUI AND MODIFIED TO JUST 
// GENERATE A TEMPFILE NAME. 


// Windows _tempnam() lets the TMP environment variable override things sent in
//  so it look like we're going to have to make a temp name by hand
//
// The user should *NOT* free the returned string.  It is stored in static space
//  and so is not valid across multiple calls to this function
//
// The names generated look like
//   c:\netscape\cache\m0.moz
//   c:\netscape\cache\m1.moz
// up to...
//   c:\netscape\cache\m9999999.moz
// after that if fails
//
char* nsForm::Temp_GenerateTempFileName(PRInt32 aMaxSize, char* file_buf)
{
  char directory[128];
  Temp_GetTempDir(&directory[0]);
  static char ext[] = ".TMP";
  static char prefix[] = "nsform";

  //  We need to base our temporary file names on time, and not on sequential
  //    addition because of the cache not being updated when the user
  //    crashes and files that have been deleted are over written with
  //    other files; bad data.
  //  The 52 valid DOS file name characters are
  //    0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_^$~!#%&-{}@`'()
  //  We will only be using the first 32 of the choices.
  //
  //  Time name format will be M+++++++.MOZ
  //    Where M is the single letter prefix (can be longer....)
  //    Where +++++++ is the 7 character time representation (a full 8.3
  //      file name will be made).
  //    Where .MOZ is the file extension to be used.
  //
  //  In the event that the time requested is the same time as the last call
  //    to this function, then the current time is incremented by one,
  //    as is the last time called to facilitate unique file names.
  //  In the event that the invented file name already exists (can't
  //    really happen statistically unless the clock is messed up or we
  //    manually incremented the time), then the times are incremented
  //    until an open name can be found.
  //
  //  time_t (the time) has 32 bits, or 4,294,967,296 combinations.
  //  We will map the 32 bits into the 7 possible characters as follows:
  //    Starting with the lsb, sets of 5 bits (32 values) will be mapped
  //      over to the appropriate file name character, and then
  //      incremented to an approprate file name character.
  //    The left over 2 bits will be mapped into the seventh file name
  //      character.
  //
  
  int i_letter, i_timechars, i_numtries = 0;
  char ca_time[8];
  time_t this_call = (time_t)0;
  
  //  We have to base the length of our time string on the length
  //    of the incoming prefix....
  //
  i_timechars = 8 - strlen(prefix);
  
  //  Infinite loop until the conditions are satisfied.
  //  There is no danger, unless every possible file name is used.
  //
  while(1)  {
    //    We used to use the time to generate this.
    //    Now, we use some crypto to avoid bug #47027
    //RNG_GenerateGlobalRandomBytes((void *)&this_call, sizeof(this_call));
	  char* output=(char *)&this_call;
    size_t len = sizeof(this_call);
	  size_t i;
	  srand((unsigned int) PR_IntervalToMilliseconds(PR_IntervalNow()));
	  for (i=0;i<len; i++) {
		  int t = rand();
		  *output = (char) (t % 256);
		  output++;
	  }

    //  Convert the time into a 7 character string.
    //  Strip only the signifigant 5 bits.
    //  We'll want to reverse the string to make it look coherent
    //    in a directory of file names.
    //
    for(i_letter = 0; i_letter < i_timechars; i_letter++) {
      ca_time[i_letter] = (char)((this_call >> (i_letter * 5)) & 0x1F);
      
      //  Convert any numbers to their equivalent ascii code
      //
      if(ca_time[i_letter] <= 9)  {
        ca_time[i_letter] += '0';
      }
      //  Convert the character to it's equivalent ascii code
      //
      else  {
        ca_time[i_letter] += 'A' - 10;
      }
    }
    
    //  End the created time string.
    //
    ca_time[i_letter] = '\0';
    
    //  Reverse the time string.
    //
// XXX fix this
#ifdef XP_PC
    _strrev(ca_time);
#endif   
    //  Create the fully qualified path and file name.
    //
    sprintf(file_buf, "%s\\%s%s%s", directory, prefix, ca_time, ext);

    //  Determine if the file exists, and mark that we've tried yet
    //    another file name (mark to be used later).
    //  
	  //  Use the system call instead of XP_Stat since we already
	  //  know the name and we don't want recursion
	  //
// XXX fix this
#ifdef XP_PC
	  struct _stat statinfo;
    int status  = _stat(file_buf, &statinfo);
#else
    int status = 1;
#endif
    i_numtries++;
    
    //  If it does not exists, we are successful, return the name.
    //
    if(status == -1)  {
      //      TRACE("Temp file name is %s\n", file_buf);
      return(file_buf);
    }
    
    //  If there is no room for additional characters in the time,
    //    we'll have to return NULL here, or we go infinite.
    //    This is a one case scenario where the requested prefix is
    //    actually 8 letters long.
    //  Infinite loops could occur with a 7, 6, 5, etc character prefixes
    //    if available files are all eaten up (rare to impossible), in
    //    which case, we should check at some arbitrary frequency of
    //    tries before we give up instead of attempting to Vulcanize
    //    this code.  Live long and prosper.
    //
    if(i_timechars == 0)  {
      break;
    }
    else if(i_numtries == 0x00FF) {
      break;
    }
  }
  
  //  Requested name is thought to be impossible to generate.
  //
  //TRACE("No more temp file names....\n");
  return(NULL);
  
}


