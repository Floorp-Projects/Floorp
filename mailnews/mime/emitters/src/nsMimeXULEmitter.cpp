/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "msgCore.h"
#include "stdio.h"
#include "nsMimeXULEmitter.h"
#include "plstr.h"
#include "nsIMimeEmitter.h"
#include "nsMailHeaders.h"
#include "nscore.h"
#include "nsEscape.h"
#include "prmem.h"
#include "nsEmitterUtils.h"
#include "nsFileStream.h"
#include "nsMimeStringResources.h"
#include "nsIMsgHeaderParser.h"
#include "nsIComponentManager.h"
#include "nsEmitterUtils.h"
#include "nsFileSpec.h"
#include "nsIRegistry.h"
#include "nsIMimeMiscStatus.h"
#include "nsIAbAddressCollecter.h"
#include "nsAbBaseCID.h"
#include "nsCOMPtr.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kMsgHeaderParserCID,		NS_MSGHEADERPARSER_CID); 
static NS_DEFINE_CID(kCAddressCollecter, NS_ABADDRESSCOLLECTER_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

nsresult NS_NewMimeXULEmitter(const nsIID& iid, void **result)
{
	nsMimeXULEmitter *obj = new nsMimeXULEmitter();
	if (obj)
		return obj->QueryInterface(iid, result);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

/*
 * nsMimeXULEmitter definitions....
 */
nsMimeXULEmitter::nsMimeXULEmitter()
{
  mCutoffValue = 3;

  // Header cache...
  mHeaderArray = new nsVoidArray();

  // Setup array for attachments
  mAttachCount = 0;
  mAttachArray = new nsVoidArray();
  mCurrentAttachment = nsnull;

  // Vars to handle the body...
  mBody = "";
  mBodyFileSpec = nsnull;
  mBodyStarted = PR_FALSE;

  if (mPrefs)
    mPrefs->GetIntPref("mailnews.max_header_display_length", &mCutoffValue);

  mMiscStatusArray = new nsVoidArray();
  BuildListOfStatusProviders();

  nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                          NULL, nsIMsgHeaderParser::GetIID(), 
                                          (void **) getter_AddRefs(mHeaderParser));
  if (NS_FAILED(rv))
    mHeaderParser = null_nsCOMPtr();
}

nsMimeXULEmitter::~nsMimeXULEmitter(void)
{
  PRInt32 i;

  if (mAttachArray)
  {
    for (i=0; i<mAttachArray->Count(); i++)
    {
      attachmentInfoType *attachInfo = (attachmentInfoType *)mAttachArray->ElementAt(i);
      if (!attachInfo)
        continue;
    
      PR_FREEIF(attachInfo->contentType);
      PR_FREEIF(attachInfo->displayName);
      PR_FREEIF(attachInfo->urlSpec);
	  delete attachInfo;
    }
    delete mAttachArray;
  }

  if (mHeaderArray)
  {
    for (i=0; i<mHeaderArray->Count(); i++)
    {
      headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(i);
      if (!headerInfo)
        continue;
    
      PR_FREEIF(headerInfo->name);
      PR_FREEIF(headerInfo->value);
	  delete headerInfo;
    }
    delete mHeaderArray;
  }

  if (mMiscStatusArray)
  {
    for (i=0; i<mMiscStatusArray->Count(); i++)
    {
      miscStatusType *statusInfo = (miscStatusType *)mHeaderArray->ElementAt(i);
      if (!statusInfo)
        continue;
    
      NS_IF_RELEASE(statusInfo->obj);
	  delete statusInfo;
    }

    delete mMiscStatusArray;
  }

  if (mBodyFileSpec)
    delete mBodyFileSpec;
}

// Attachment handling routines
nsresult
nsMimeXULEmitter::StartAttachment(const char *name, const char *contentType, const char *url)
{
  // Ok, now we will setup the attachment info 
  mCurrentAttachment = (attachmentInfoType *) PR_NEWZAP(attachmentInfoType);
  if ( (mCurrentAttachment) && mAttachArray)
  {
    ++mAttachCount;

    mCurrentAttachment->displayName = nsCRT::strdup(name);
    mCurrentAttachment->urlSpec = nsCRT::strdup(url);
    mCurrentAttachment->contentType = nsCRT::strdup(contentType);
  }

  return NS_OK;
}

nsresult
nsMimeXULEmitter::EndAttachment()
{
  // Ok, add the attachment info to the attachment array...
  if ( (mCurrentAttachment) && (mAttachArray) )
  {
    mAttachArray->AppendElement(mCurrentAttachment);
    mCurrentAttachment = nsnull;
  }

  return NS_OK;
}

// Attachment handling routines
nsresult
nsMimeXULEmitter::StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset)
{
  mBody.Append("<HTML>");

  mBody.Append("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=");
  mBody.Append(outCharset);
  mBody.Append("\">");

  mBodyStarted = PR_TRUE;
  return NS_OK;
}

nsresult
nsMimeXULEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  if (size == 0)
  {
    *amountWritten = 0;
    return NS_OK;
  }

  mBody.Append(buf, size);
  *amountWritten = size;

  return NS_OK;
}

#define TEMP_FILE_PREFIX    "nsMimeBody"

//
// RICHIE - We need to find a way to tell the webshell to delete the file once it is
// loaded. Until then, we will continually cleaup behind ourselves....ugh..
nsresult
nsMimeXULEmitter::OhTheHumanityCleanupTempFileHack()
{
  // Age old question, where to store temp files....ugh!
  char  *tDir = GetTheTempDirectoryOnTheSystem();
  if (!tDir)
    return NS_OK;

  nsFileSpec tempDir(tDir);
  if (tempDir.Exists())
  {
    for (nsDirectoryIterator i(tempDir, PR_FALSE); i.Exists(); i++) 
    {
      nsFileSpec possibleTempFile = i.Spec();
      char       *filename = possibleTempFile.GetLeafName();
    
      if ((nsCRT::strncmp(TEMP_FILE_PREFIX, filename, nsCRT::strlen(TEMP_FILE_PREFIX)) == 0) && 
          (nsCRT::strlen(filename) > nsCRT::strlen(TEMP_FILE_PREFIX)))
      {
        possibleTempFile.Delete(PR_FALSE);
      }

      nsCRT::free(filename);
      filename = nsnull;
    }
  }

  PR_FREEIF(tDir);
  return NS_OK;
}

nsresult
nsMimeXULEmitter::EndBody()
{
  OhTheHumanityCleanupTempFileHack();

  mBody.Append("</HTML>");
  mBodyStarted = PR_FALSE;

  // Now that the body is complete, we are going to create a temp file and 
  // write it out to disk. The nsFileSpec will be stored in the object for 
  // writing to the output stream on the Complete() call.  
  mBodyFileSpec = nsMsgCreateTempFileSpec("nsMimeBody.html");
  if (!mBodyFileSpec)
    return NS_ERROR_NULL_POINTER;

  nsOutputFileStream tempOutfile(*mBodyFileSpec);
  if (! tempOutfile.is_open())
    return NS_ERROR_UNEXPECTED;
  
  tempOutfile.write((const char *) mBody, mBody.Length());
  tempOutfile.close();
  return NS_OK;
}

nsresult
nsMimeXULEmitter::AddHeaderFieldHTML(const char *field, const char *value)
{
  if ( (!field) || (!value) )
    return NS_OK;

  //
  // This is a check to see what the pref is for header display. If
  // We should only output stuff that corresponds with that setting.
  //
  if (!EmitThisHeaderForPrefSetting(mHeaderDisplayType, field))
    return NS_OK;

  char  *newValue = nsEscapeHTML(value);
  if (!newValue)
    return NS_OK;

  UtilityWrite("<TR>");

  UtilityWrite("<td>");
  UtilityWrite("<div align=right>");
  UtilityWrite("<B>");

  // Here is where we are going to try to L10N the tagName so we will always
  // get a field name next to an emitted header value. Note: Default will always
  // be the name of the header itself.
  //
  nsCAutoString  newTagName(field);
  newTagName.CompressWhitespace(PR_TRUE, PR_TRUE);
  newTagName.ToUpperCase();

  char *l10nTagName = LocalizeHeaderName((const char *) newTagName, field);
  if ( (!l10nTagName) || (!*l10nTagName) )
    UtilityWrite(field);
  else
  {
    UtilityWrite(l10nTagName);
    PR_FREEIF(l10nTagName);
  }

  // Now write out the actual value itself and move on!
  //
  UtilityWrite(":");
  UtilityWrite("</B>");
  UtilityWrite("</div>");
  UtilityWrite("</td>");

  UtilityWrite("<td>");
  UtilityWrite(newValue);
  UtilityWrite("</td>");

  UtilityWrite("</TR>");

  nsCRT::free(newValue);
  return NS_OK;
}

//
// Find a cached header! Note: Do NOT free this value!
//
char *
nsMimeXULEmitter::GetHeaderValue(const char *aHeaderName)
{
  PRInt32     i;
  char        *retVal = nsnull;

  if (!mHeaderArray)
    return nsnull;

  for (i=0; i<mHeaderArray->Count(); i++)
  {
    headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(i);
    if ( (!headerInfo) || (!headerInfo->name) || (!(*headerInfo->name)) )
      continue;
    
    if (!nsCRT::strcasecmp(aHeaderName, headerInfo->name))
    {
      retVal = headerInfo->value;
      break;
    }
  }

  return retVal;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// Ok, these are the routines that will need to be modified to alter the 
// appearance of the XUL display for the message. Everything above this line
// should just be support routines for this output.
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

// 
// This should be any of the XUL that has to be part of the header for the
// display. This will include style sheet information, etc...
//
nsresult
nsMimeXULEmitter::WriteXULHeader(const char *msgID)
{
  if ( (!msgID) || (!*msgID) )
    msgID = "none";
    
  char  *newValue = nsEscapeHTML(msgID);
  if (!newValue)
    return NS_ERROR_OUT_OF_MEMORY;
    
  UtilityWriteCRLF("<?xml version=\"1.0\"?>");

  if (mHeaderDisplayType == nsMimeHeaderDisplayTypes::MicroHeaders)
    UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://messenger/skin/mailheader-micro.css\" type=\"text/css\"?>");
  else if (mHeaderDisplayType == nsMimeHeaderDisplayTypes::NormalHeaders)
    UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://messenger/skin/mailheader-normal.css\" type=\"text/css\"?>");
  else /* AllHeaders */
    UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://messenger/skin/mailheader-all.css\" type=\"text/css\"?>");

  // Make it look consistent...
  UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://global/skin/\" type=\"text/css\"?>");


  // Now, the XUL window!
  UtilityWriteCRLF("<window ");
  UtilityWriteCRLF("xmlns:html=\"http://www.w3.org/TR/REC-html40\" ");
  UtilityWriteCRLF("xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" ");
  UtilityWriteCRLF("xmlns=\"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul\" ");
  DoWindowStatusProcessing();
  UtilityWriteCRLF("align=\"vertical\" flex=\"1\"> ");

  // Output the message ID to make it query-able via the DOM
  UtilityWrite("<message id=\"");
  UtilityWrite(newValue);
  nsCRT::free(newValue);
  UtilityWriteCRLF("\"/>");

  // Now, the JavaScript...
  UtilityWriteCRLF("<html:script language=\"javascript\" src=\"chrome://messenger/content/attach.js\"/>");
  UtilityWriteCRLF("<html:script language=\"javascript\" src=\"chrome://messenger/content/mime.js\"/>");
  DoGlobalStatusProcessing();

  return NS_OK;
}

//
// This is called at the start of the header block for all header information in ANY
// AND ALL MESSAGES (yes, quoted, attached, etc...) So, we need to handle these differently
// if they are the rood message as opposed to quoted message. If rootMailHeader is set to
// PR_TRUE, this is the root document, else it is a header in the body somewhere
//
nsresult
nsMimeXULEmitter::StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                              const char *outCharset)
{
  mDocHeader = rootMailHeader;

  if (!mDocHeader)
  {
    UtilityWriteCRLF("<BLOCKQUOTE><table BORDER=0 BGCOLOR=\"#CCCCCC\" >");
  }
  else
  {
    WriteXULHeader(msgID);
  }

  return NS_OK; 
}

nsresult
nsMimeXULEmitter::DoSpecialSenderProcessing(const char *field, const char *value)
{
	nsresult rv = NS_OK;
	if (!nsCRT::strcmp(field, "From"))
	{
		NS_WITH_SERVICE(nsIAbAddressCollecter, addressCollecter,
						kCAddressCollecter, &rv);
		if (NS_SUCCEEDED(rv) && addressCollecter)
			addressCollecter->CollectAddress(value);
	}
	return rv;
}

//
// This will be called for every header field...for now, we are calling the WriteXULTag for
// each of these calls.
//
nsresult
nsMimeXULEmitter::AddHeaderField(const char *field, const char *value)
{
  if ( (!field) || (!value) )
    return NS_OK;

  if ( (mDocHeader) && (!nsCRT::strcmp(field, HEADER_FROM)) )
    DoSpecialSenderProcessing(field, value);

  if (!mDocHeader)
    return AddHeaderFieldHTML(field, value);
  else
  { 
    // This is the main header so we do caching for XUL later!
    // Ok, now we will setup the header info for the header array!
    headerInfoType  *ptr = (headerInfoType *) PR_NEWZAP(headerInfoType);
    if ( (ptr) && mHeaderArray)
    {
      ptr->name = nsCRT::strdup(field);
      ptr->value = nsCRT::strdup(value);
      mHeaderArray->AppendElement(ptr);
    }

    return NS_OK;
  }
}

//
// This finalizes the header field being parsed.
//
nsresult
nsMimeXULEmitter::EndHeader()
{
  if (!mDocHeader)
    UtilityWriteCRLF("</TABLE></BLOCKQUOTE>");
  
  // Nothing to do in the case of the actual Envelope. This will be done
  // on Complete().

  return NS_OK; 
}

//
// This is called at teardown time, but since we have been cacheing lots of 
// good information about the mail message (headers, attachments and body), this 
// is the time that we have to do a lot of completion processing...
// such as writing the XUL for the attachments and the XUL for the body
// display
//
nsresult
nsMimeXULEmitter::Complete()
{
  // Start off the header as a toolbox!
  UtilityWriteCRLF("<toolbox>");

  // Start with the subject, from date info!
  DumpSubjectFromDate();

  // Continue with the to and cc headers
  DumpToCC();

  // do the rest of the headers, but these will only be written if
  // the user has the "show all headers" pref set
  DumpRestOfHeaders();

  // Now close out the header toolbox
  UtilityWriteCRLF("</toolbox>");

  // Now process the body...
  DumpBody();

  // Finalize the XUL document...
  UtilityWriteCRLF("</window>");

  return nsMimeBaseEmitter::Complete();
}


nsresult
nsMimeXULEmitter::DumpAttachmentMenu()
{
  nsresult rv;

  if ( (!mAttachArray) || (mAttachArray->Count() <= 0) )
    return NS_OK;

  char *buttonXUL = PR_smprintf(
                      "<titledbutton src=\"chrome://messenger/skin/attach.gif\" value=\"%d\" align=\"right\"/>", 
                      mAttachArray->Count());

  if ( (!buttonXUL) || (!*buttonXUL) )
    return NS_OK;

  UtilityWriteCRLF("<box align=\"horizontal\">");

  char *escapedUrl = nsnull;
  char *escapedName = nsnull;
  nsCOMPtr<nsIMsgMessageUrl> messageUrl;
  char *urlString = nsnull;

  // Now we can finally write out the attachment information...  
  if (mAttachArray->Count() > 0)
  {
    PRInt32     i;
    UtilityWriteCRLF("<menu name=\"attachment-menu\">");
    UtilityWriteCRLF(buttonXUL);
    UtilityWriteCRLF("<menupopup>");    

    for (i=0; i<mAttachArray->Count(); i++)
    {
      attachmentInfoType *attachInfo = (attachmentInfoType *)mAttachArray->ElementAt(i);
      if (!attachInfo)
        continue;
      
      UtilityWrite("<menuitem value=\"");

      escapedName = nsEscape(attachInfo->displayName, url_Path);
      if (escapedName)
      {
        UtilityWrite(escapedName);
      }
      else
        UtilityWrite(attachInfo->displayName);

      UtilityWrite("\" oncommand=\"OpenAttachURL('");
      escapedUrl = nsEscape(attachInfo->urlSpec, url_Path);
      if (escapedUrl)
      {
        UtilityWrite(escapedUrl);
        nsCRT::free(escapedUrl);
      }
      else
      {
        UtilityWrite(attachInfo->urlSpec);
      }

      UtilityWrite("','");
      if (escapedName)
      {
        UtilityWrite(escapedName);
      }
      else
        UtilityWrite(attachInfo->displayName);
      UtilityWrite("','");
      
      nsCOMPtr<nsIMsgMessageUrl> messageUrl = do_QueryInterface(mURL, &rv);
      if (NS_SUCCEEDED(rv))
        rv = messageUrl->GetURI(&urlString);
      if (NS_SUCCEEDED(rv) && urlString)
      {
        UtilityWrite(urlString);
        nsCRT::free(urlString);
        urlString = nsnull;
      }

      UtilityWriteCRLF("' );\"  />");
      PR_FREEIF(escapedName);
    }
    UtilityWriteCRLF("</menupopup>");
    UtilityWriteCRLF("</menu>");
  }

  UtilityWriteCRLF("</box>");
  PR_FREEIF(buttonXUL);

  return NS_OK;
}

nsresult
nsMimeXULEmitter::DumpAddBookIcon(char *fromLine)
{
  char        *email = nsnull;
  char        *name = nsnull;
  PRUint32    numAddresses;
	char	      *names;
	char	      *addresses;
  nsresult    rv;

  if (!fromLine)
	  return NS_OK;

  UtilityWriteCRLF("<box align=\"horizontal\">");

  if (mHeaderParser)
    rv = mHeaderParser->ParseHeaderAddresses ("UTF-8", fromLine, 
                                              &names, &addresses, &numAddresses);  
  if (NS_SUCCEEDED(rv))
  {
	  name = names;
    email = addresses;
  }
  else
  {
    name = fromLine;
    email = fromLine;
  }

  nsCAutoString  newName;
  char *newNameValue = nsEscapeHTML(name);
  if (newNameValue) 
  {
    newName.SetString(newNameValue);
    nsCRT::free(newNameValue);
  }
  else
  {
    newName.SetString(name);
  }

  // Strip off extra quotes...
  newName.Trim("\"");

  UtilityWrite("<titledbutton src=\"chrome://messenger/skin/addcard.gif\" ");
  UtilityWrite("onclick=\"AddToAddressBook('");
  UtilityWrite(email);
  UtilityWrite("', '");
  UtilityWrite((const char *) newName);
  UtilityWriteCRLF("');\"/>");

  UtilityWriteCRLF("</box>");

  PR_FREEIF(names);
  PR_FREEIF(addresses);
  return NS_OK;
}

nsresult
nsMimeXULEmitter::DumpBody()
{
  // Now we will write out the XUL/IFRAME line for the mBody which
  // we have cached to disk.
  //
  char  *url = nsnull;
  if (mBodyFileSpec)
    url = nsMimePlatformFileToURL(*mBodyFileSpec);

  UtilityWrite("<html:iframe id=\"mail-body-frame\" type=\"content-primary\" src=\"");
  UtilityWrite(url);
  UtilityWriteCRLF("\" border=\"0\" scrolling=\"auto\" resize=\"yes\" width=\"100%\" flex=\"1\"/>");
  PR_FREEIF(url);
  return NS_OK;
}

nsresult
nsMimeXULEmitter::OutputGenericHeader(const char *aHeaderVal)
{
  char      *val = GetHeaderValue(aHeaderVal);
  nsresult  rv;

  if (val)
  {
    UtilityWriteCRLF("<box>");
    rv = WriteXULTag(aHeaderVal, val);
    UtilityWriteCRLF("</box>");
    return rv;
  }
  else
    return NS_ERROR_FAILURE;
}

nsresult
nsMimeXULEmitter::DumpSubjectFromDate()
{
  UtilityWriteCRLF("<toolbar>");
  UtilityWriteCRLF("<box name=\"header-splitter\" align=\"horizontal\" flex=\"1\" >");

      UtilityWriteCRLF("<box name=\"header-part1\" align=\"vertical\" flex=\"1\">");
      UtilityWriteCRLF("<spring flex=\"2\"/>");

          // This is the envelope information
          OutputGenericHeader(HEADER_SUBJECT);
          OutputGenericHeader(HEADER_FROM);
          OutputGenericHeader(HEADER_DATE);

      UtilityWriteCRLF("<spring flex=\"2\"/>");
      UtilityWriteCRLF("</box>");


      UtilityWriteCRLF("<box name=\"header-attachment\" align=\"horizontal\" flex=\"1\">");
      UtilityWriteCRLF("<spring flex=\"1\"/>");

        // Now the addbook and attachment icons need to be displayed
        DumpAddBookIcon(GetHeaderValue(HEADER_FROM));
        DumpAttachmentMenu();

      UtilityWriteCRLF("<spring flex=\"1\"/>");
      UtilityWriteCRLF("</box>");

  UtilityWriteCRLF("</box>");
  UtilityWriteCRLF("</toolbar>");

  return NS_OK;
}

nsresult
nsMimeXULEmitter::DumpToCC()
{
  char * toField = GetHeaderValue(HEADER_TO);
  char * ccField = GetHeaderValue(HEADER_CC);
  char * bccField = GetHeaderValue(HEADER_BCC);
  char * newsgroupField = GetHeaderValue(HEADER_NEWSGROUPS);

  // only dump these fields if we have at least one of them! When displaying news
  // messages that didn't have a to or cc field, we'd always get an empty box
  // which looked weird.
  if (toField || ccField || bccField || newsgroupField)
  {
    UtilityWriteCRLF("<toolbar>");
    UtilityWriteCRLF("<box name=\"header-part2\" align=\"vertical\" flex=\"1\">");

    OutputGenericHeader(HEADER_TO);
    OutputGenericHeader(HEADER_CC);
    OutputGenericHeader(HEADER_BCC);
    OutputGenericHeader(HEADER_NEWSGROUPS);

    UtilityWriteCRLF("</box>");
    UtilityWriteCRLF("</toolbar>");
  }

  return NS_OK;
}

nsresult
nsMimeXULEmitter::DumpRestOfHeaders()
{
  PRInt32     i;

  if (mHeaderDisplayType != nsMimeHeaderDisplayTypes::AllHeaders)
  {
    // For now, lets advertise the fact that 5.0 sent this message.
    char  *userAgent = nsMimeXULEmitter::GetHeaderValue(HEADER_USER_AGENT);

    if (userAgent)
    {
      char  *compVal = "Mozilla 5.0";
      if (!nsCRT::strncasecmp(userAgent, compVal, nsCRT::strlen(compVal)))
      {
        UtilityWriteCRLF("<toolbar>");
        UtilityWriteCRLF("<box name=\"header-seamonkey\" align=\"vertical\" flex=\"1\">");
        UtilityWriteCRLF("<box>");
        WriteXULTag(HEADER_USER_AGENT, userAgent);
        UtilityWriteCRLF("</box>");
        UtilityWriteCRLF("</box>");        
        UtilityWriteCRLF("</toolbar>");
      }
    }

    return NS_OK;
  }


  UtilityWriteCRLF("<toolbar>");

    UtilityWriteCRLF("<box name=\"header-part3\" align=\"vertical\" flex=\"1\">");

      for (i=0; i<mHeaderArray->Count(); i++)
      {
        headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(i);
        if ( (!headerInfo) || (!headerInfo->name) || (!(*headerInfo->name)) ||
              (!headerInfo->value) || (!(*headerInfo->value)))
          continue;
    
        if ( (!nsCRT::strcasecmp(HEADER_SUBJECT, headerInfo->name)) ||
             (!nsCRT::strcasecmp(HEADER_DATE, headerInfo->name)) ||
             (!nsCRT::strcasecmp(HEADER_FROM, headerInfo->name)) ||
             (!nsCRT::strcasecmp(HEADER_TO, headerInfo->name)) ||
             (!nsCRT::strcasecmp(HEADER_CC, headerInfo->name)) )
           continue;

        UtilityWriteCRLF("<box>");
        WriteXULTag(headerInfo->name, headerInfo->value);
        UtilityWriteCRLF("</box>");
      }

    UtilityWriteCRLF("</box>");

  UtilityWriteCRLF("</toolbar>");
  return NS_OK;
}

//
// This is a general routine that will dispatch the writing of headers to
// the correct method
//
nsresult
nsMimeXULEmitter::WriteXULTag(const char *tagName, const char *value)
{
  if ( (!nsCRT::strcasecmp(HEADER_FROM, tagName)) ||
       (!nsCRT::strcasecmp(HEADER_CC, tagName)) ||
       (!nsCRT::strcasecmp(HEADER_TO, tagName)) ||
       (!nsCRT::strcasecmp(HEADER_BCC, tagName)) )
    return WriteEmailAddrXULTag(tagName, value);
  else
    return WriteMiscXULTag(tagName, value);
}


nsresult
nsMimeXULEmitter::WriteXULTagPrefix(const char *tagName, const char *value)
{
  nsCAutoString  newTagName(tagName);
  newTagName.CompressWhitespace(PR_TRUE, PR_TRUE);

  newTagName.ToUpperCase();

  UtilityWrite("<header field=\"");
  UtilityWrite(newTagName);
  UtilityWrite("\">");

  // Here is where we are going to try to L10N the tagName so we will always
  // get a field name next to an emitted header value. Note: Default will always
  // be the name of the header itself.
  //
  UtilityWriteCRLF("<html:table BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\" >");  
  UtilityWriteCRLF("<html:tr VALIGN=\"TOP\">");
  UtilityWriteCRLF("<html:td>");

  UtilityWrite("<headerdisplayname>");
  char *l10nTagName = LocalizeHeaderName((const char *) newTagName, tagName);
  if ( (!l10nTagName) || (!*l10nTagName) )
    UtilityWrite(tagName);
  else
  {
    UtilityWrite(l10nTagName);
    PR_FREEIF(l10nTagName);
  }

  UtilityWrite(": ");
  UtilityWriteCRLF("</headerdisplayname>");

  UtilityWriteCRLF("</html:td>");
  return NS_OK;
}

nsresult
nsMimeXULEmitter::WriteXULTagPostfix(const char *tagName, const char *value)
{
  UtilityWriteCRLF("</html:tr>");
  UtilityWriteCRLF("</html:table>");

  // Finalize and write out end of headers...
  UtilityWriteCRLF("</header>");
  return NS_OK;
}

//
// This is the routine that is responsible for parsing and writing email
// header. (i.e. From:, CC:, etc...) This will parse apart the addresses
// and do things like make them links for easy adding to the address book
//
nsresult
nsMimeXULEmitter::WriteEmailAddrXULTag(const char *tagName, const char *value)
{
  if ( (!value) || (!*value) )
    return NS_OK;

  nsCAutoString  newTagName(tagName);
  newTagName.CompressWhitespace(PR_TRUE, PR_TRUE);
  newTagName.ToUpperCase();

  WriteXULTagPrefix(tagName, value);

  // Ok, now this is where we need to loop through all of the addresses and
  // do interesting things with the contents.
  //
  UtilityWriteCRLF("<html:td>"); 
  OutputEmailAddresses((const char *) newTagName, value);
  UtilityWriteCRLF("</html:td>");

  WriteXULTagPostfix(tagName, value);
  return NS_OK;
}

//
// This is a header tag (i.e. tagname: "From", value: rhp@netscape.com. Do the right 
// thing here to display this header in the XUL output.
//
nsresult
nsMimeXULEmitter::WriteMiscXULTag(const char *tagName, const char *value)
{
  if ( (!value) || (!*value) )
    return NS_OK;

  WriteXULTagPrefix(tagName, value);

  UtilityWriteCRLF("<html:td>");

  // Now write out the actual value itself and move on!
  char  *newValue = nsEscapeHTML(value);
  if (newValue) 
  {
    UtilityWrite(newValue);
    nsCRT::free(newValue);
  }
  else
  {
    UtilityWrite(value);
  }
  ////

  UtilityWriteCRLF("</html:td>");

  WriteXULTagPostfix(tagName, value);
  return NS_OK;
}

nsresult
nsMimeXULEmitter::DoGlobalStatusProcessing()
{
  char    *retval = nsnull;

  if (mMiscStatusArray)
  {
    for (PRInt32 i=0; i<mMiscStatusArray->Count(); i++)
    {
      retval = nsnull;
      miscStatusType *statusInfo = (miscStatusType *)mMiscStatusArray->ElementAt(i);
      if (statusInfo->obj)
      {
        if (NS_SUCCEEDED(statusInfo->obj->GetGlobalXULandJS(&retval)))
        {
          if ( (retval) && (*retval) )
            UtilityWriteCRLF(retval);

          PR_FREEIF(retval);
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsMimeXULEmitter::DoWindowStatusProcessing()
{
  char    *retval = nsnull;

  if (mMiscStatusArray)
  {
    for (PRInt32 i=0; i<mMiscStatusArray->Count(); i++)
    {
      retval = nsnull;
      miscStatusType *statusInfo = (miscStatusType *)mMiscStatusArray->ElementAt(i);
      if (statusInfo->obj)
      {
        if (NS_SUCCEEDED(statusInfo->obj->GetWindowXULandJS(&retval)))
        {
          if ( (retval) && (*retval) )
            UtilityWriteCRLF(retval);

          PR_FREEIF(retval);
        }
      }
    }
  }

  return NS_OK;
}

nsresult       
nsMimeXULEmitter::OutputEmailAddresses(const char *aHeader, const char *aEmailAddrs)
{
	PRUint32    numAddresses;
	char	      *names;
	char	      *addresses;
 
  if ( (!mHeaderParser) ||
       NS_FAILED(mHeaderParser->ParseHeaderAddresses ("UTF-8", aEmailAddrs, 
                 &names, &addresses, &numAddresses)) )
  {
    char *newValue = nsEscapeHTML(aEmailAddrs);
    if (newValue) 
    {
      UtilityWrite(newValue);
      nsCRT::free(newValue);
    }
    return NS_OK;
  }

	char        *curName = names;
  char        *curAddress = addresses;

  if (numAddresses > (PRUint32) mCutoffValue)
  {
    UtilityWrite("<html:div id=\"SHORT");
    UtilityWrite(aHeader);
    UtilityWriteCRLF("\" style=\"display: block;\">");

    for (PRUint32 i = 0; i < (PRUint32) mCutoffValue; i++)
    {
      ProcessSingleEmailEntry(aHeader, curName, curAddress);
      
      if (i != (numAddresses-1))
        UtilityWrite(",&#160;");
      
      if ( ( ((i+1) % 2) == 0) && ((i+1) != (PRUint32) mCutoffValue))
      {
        UtilityWrite("<html:BR/>");
      }
      
      curName += strlen(curName) + 1;
      curAddress += strlen(curAddress) + 1;
    }

    UtilityWrite("<titledbutton class=\"SHORT");
    UtilityWrite(aHeader);
    UtilityWrite("_button\" src=\"chrome://messenger/skin/more.gif\" onclick=\"ShowLong('");
    UtilityWrite(aHeader);
    UtilityWriteCRLF("');\" style=\"vertical-align: text-top;\"/>");
  
    UtilityWrite("</html:div>");
    UtilityWrite("<html:div id=\"LONG");
    UtilityWrite(aHeader);
    UtilityWriteCRLF("\" style=\"display: none;\">");
  }

	curName = names;
  curAddress = addresses;
  for (PRUint32 i = 0; i < numAddresses; i++)
  {
    ProcessSingleEmailEntry(aHeader, curName, curAddress);

    if (i != (numAddresses-1))
      UtilityWrite(",&#160;");

    if ( ( ((i+1) % 2) == 0) && (i != (numAddresses-1)))
    {
      UtilityWrite("<html:BR/>");
    }

    curName += strlen(curName) + 1;
    curAddress += strlen(curAddress) + 1;
  }

  if (numAddresses > (PRUint32) mCutoffValue)
  {
    UtilityWrite("<titledbutton class=\"LONG");
    UtilityWrite(aHeader);
    UtilityWrite("_button\" src=\"chrome://messenger/skin/less.gif\" onclick=\"ShowShort('");
    UtilityWrite(aHeader);
    UtilityWriteCRLF("');\" style=\"vertical-align: text-top;\"/>");

    UtilityWriteCRLF("</html:div>");
  }

  PR_FREEIF(addresses);
  PR_FREEIF(names);

  return NS_OK;
}

nsresult
nsMimeXULEmitter::ProcessSingleEmailEntry(const char *curHeader, char *curName, char *curAddress)
{
  char      *link = nsnull;
  char      *tLink = nsnull;
  nsCAutoString  workName (curName);
  nsCAutoString  workAddr(curAddress);
  nsCAutoString  workString(curName); 

  workName.Trim("\"");   
  char * htmlString = nsEscapeHTML(workName);
  if (htmlString)
  {
    workName = htmlString;
    nsCRT::free(htmlString);
  }

  workAddr.Trim("\"");

  // tLink = PR_smprintf("addbook:add?vcard=begin%%3Avcard%%0Afn%%3A%s%%0Aemail%%3Binternet%%3A%s%%0Aend%%3Avcard%%0A", 
  //                    (workName ? workName : workAddr), workAddr);
  tLink = PR_smprintf("mailto:%s", (const char *) workAddr); 
  if (tLink)
    link = nsEscapeHTML(tLink);
  if (link)
  {
    UtilityWrite("<html:a href=\"");
    UtilityWrite(link);
    UtilityWrite("\">");
  }

  if (!workName.IsEmpty())
    UtilityWrite((const char *) workName);
  else
    UtilityWrite(curName);

  UtilityWrite(" &lt;");
  UtilityWrite(curAddress);
  UtilityWrite("&gt;");
  
  // End link stuff here...
  if (link)
    UtilityWriteCRLF("</html:a>");

  nsCRT::free(link);
  PR_FREEIF(tLink);

  // Misc here
  char    *xul;
  if (mMiscStatusArray)
  {
    for (PRInt32 i=0; i<mMiscStatusArray->Count(); i++)
    {
      xul = nsnull;
      miscStatusType *statusInfo = (miscStatusType *)mMiscStatusArray->ElementAt(i);
      if (statusInfo->obj)
      {
        if (NS_SUCCEEDED(statusInfo->obj->GetIndividualXUL(curHeader, workName, workAddr, &xul)))
        {
          if ( (xul) && (*xul) )
            UtilityWriteCRLF(xul);

          PR_FREEIF(xul);
        }
      }
    }
  }

  return NS_OK;  
}

nsresult
nsMimeXULEmitter::BuildListOfStatusProviders() 
{
  nsresult rv;

  // enumerate the registry subkeys
  nsRegistryKey      key;
  nsCOMPtr<nsIEnumerator> components;
  miscStatusType        *newInfo = nsnull;

  NS_WITH_SERVICE(nsIRegistry, registry, NS_REGISTRY_PROGID, &rv); 
  if (NS_FAILED(rv)) 
    return rv;
  
  rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
  if (NS_FAILED(rv)) 
    return rv;
  
  rv = registry->GetSubtree(nsIRegistry::Common, NS_IMIME_MISC_STATUS_KEY, &key);
  if (NS_FAILED(rv)) 
    return rv;
  
  rv = registry->EnumerateSubtrees(key, getter_AddRefs(components));
  if (NS_FAILED(rv)) 
    return rv;
  
  // go ahead and enumerate through.
  nsCAutoString actualProgID;
  rv = components->First();
  while (NS_SUCCEEDED(rv) && (NS_OK != components->IsDone()))
  {
    nsCOMPtr<nsISupports> base;
    
    rv = components->CurrentItem(getter_AddRefs(base));
    if (NS_FAILED(rv)) 
      return rv;
    
    nsCOMPtr<nsIRegistryNode> node;
    nsIID nodeIID = NS_IREGISTRYNODE_IID;
    node = do_QueryInterface(base, &rv);
    if (NS_FAILED(rv)) 
      return rv;
    
    nsXPIDLCString name;
    rv = node->GetName(getter_Copies(name));
    if (NS_FAILED(rv)) 
      return rv;
    
    actualProgID = NS_IMIME_MISC_STATUS_KEY;
    actualProgID.Append(name);
    
    // now we've got the PROGID, let's add it to the list...
    newInfo = (miscStatusType *)PR_NEWZAP(miscStatusType);
    if (newInfo)
    {
      newInfo->obj = GetStatusObjForProgID(actualProgID);
      if (newInfo->obj)
      {
        newInfo->progID = actualProgID;
        mMiscStatusArray->AppendElement(newInfo);
      }
    }

    rv = components->Next();
  }
  
  registry->Close();
 
  return NS_OK;
}

nsIMimeMiscStatus *
nsMimeXULEmitter::GetStatusObjForProgID(nsCString aProgID)
{
  nsresult            rv = NS_OK;
  nsIMimeMiscStatus   *returnObj = nsnull;
  nsISupports         *obj = nsnull;

  NS_WITH_SERVICE(nsIComponentManager, comMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) 
    return nsnull;
  
  nsCID         cid;
  rv = comMgr->ProgIDToCLSID(aProgID, &cid);
  if (NS_FAILED(rv))
    return nsnull;

  rv = comMgr->CreateInstance(cid, nsnull, NS_GET_IID(nsIMimeMiscStatus), (void**)&obj);  
  if (NS_FAILED(rv))
    return nsnull;
  else
    return returnObj;
}

NS_IMETHODIMP
nsMimeXULEmitter::Write(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  unsigned int        written = 0;
  PRUint32            rc = 0;
  PRUint32            needToWrite;

 // *m_outputFile << buf;
  //
  // Make sure that the buffer we are "pushing" into has enough room
  // for the write operation. If not, we have to buffer, return, and get
  // it on the next time through
  //
  *amountWritten = 0;

  needToWrite = mBufferMgr->GetSize();
  // First, handle any old buffer data...
  if (needToWrite > 0)
  {
    rc += mOutStream->Write(mBufferMgr->GetBuffer(), 
                            needToWrite, &written);
    mTotalWritten += written;
    mBufferMgr->ReduceBuffer(written);
//    mOutListener->OnDataAvailable(mChannel, mURL, mInputStream, 0, written);

    *amountWritten = written;

    // if we couldn't write all the old data, buffer the new data
    // and return
    if (mBufferMgr->GetSize() > 0)
    {
      mBufferMgr->IncreaseBuffer(buf, size);
      return NS_OK;
    }
  }

  // if we get here, we are dealing with new data...try to write
  // and then do the right thing...
  //
  // Note: if the body has been started, we shouldn't write to the
  // output stream, but rather, just append to the body buffer.
  //
  if (mBodyStarted)
  {
    mBody.Append(buf, size);
    rc = size;
    written = size;
  }
  else
    rc = mOutStream->Write(buf, size, &written);

  *amountWritten = written;
  mTotalWritten += written;

  if (written < size)
    mBufferMgr->IncreaseBuffer(buf+written, (size-written));

  // Only call the listener if we wrote data into the stream.
//  if ((!mBodyStarted) && (mOutListener))
    //mOutListener->OnDataAvailable(mChannel, mURL, mInputStream, 0, written);

  return rc;
}


