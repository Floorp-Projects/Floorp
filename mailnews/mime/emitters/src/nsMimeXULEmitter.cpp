/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsSpecialSystemDirectory.h"
#include "plbase64.h"
#include "nsIMimeStreamConverter.h"
#include "prprf.h"

static NS_DEFINE_CID(kMsgHeaderParserCID,		NS_MSGHEADERPARSER_CID); 
static NS_DEFINE_CID(kCAddressCollecter, NS_ABADDRESSCOLLECTER_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

/*
 * nsMimeXULEmitter definitions....
 */
nsMimeXULEmitter::nsMimeXULEmitter()
{
  mCutoffValue = 3;

  // Vars to handle the body...
  mBody = "";
  mBodyStarted = PR_FALSE;

  if (mPrefs)
    mPrefs->GetIntPref("mailnews.max_header_display_length", &mCutoffValue);

  mMiscStatusArray = new nsVoidArray();
  BuildListOfStatusProviders();

  nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                          NULL, NS_GET_IID(nsIMsgHeaderParser), 
                                          (void **) getter_AddRefs(mHeaderParser));
  if (NS_FAILED(rv))
    mHeaderParser = null_nsCOMPtr();
}

nsMimeXULEmitter::~nsMimeXULEmitter(void)
{
  PRInt32     i;

  if (mMiscStatusArray)
  {
    for (i=0; i<mMiscStatusArray->Count(); i++)
    {
      miscStatusType *statusInfo = (miscStatusType *)mMiscStatusArray->ElementAt(i);
      if (!statusInfo)
        continue;
    
      NS_IF_RELEASE(statusInfo->obj);
	    delete statusInfo;
    }

    delete mMiscStatusArray;
  }
}

// Attachment handling routines
nsresult
nsMimeXULEmitter::StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset)
{
  // Setup everything for META tags and stylesheet info!
  mBody.Append("<HTML>");
  mBody.Append("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=");
  mBody.Append(outCharset);
  mBody.Append("\">");

  // Now for some style...
  mBody.Append("<LINK REL=\"STYLESHEET\" HREF=\"chrome://messenger/skin/messageBody.css\">");
  mBody.Append("<LINK REL=\"STYLESHEET\" HREF=\"chrome://global/skin\">");

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

nsresult
nsMimeXULEmitter::EndBody()
{
  mBody.Append("</HTML>");
  mBodyStarted = PR_FALSE;
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

  nsCOMPtr<nsIRegistry> registry = 
           do_GetService(NS_REGISTRY_CONTRACTID, &rv); 
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
  nsCAutoString actualContractID;
  rv = components->First();
  while (NS_SUCCEEDED(rv) && (NS_OK != components->IsDone()))
  {
    nsCOMPtr<nsISupports> base;
    
    rv = components->CurrentItem(getter_AddRefs(base));
    if (NS_FAILED(rv)) 
      return rv;
    
    nsCOMPtr<nsIRegistryNode> node;
    node = do_QueryInterface(base, &rv);
    if (NS_FAILED(rv)) 
      return rv;
    
    nsXPIDLCString name;
    rv = node->GetNameUTF8(getter_Copies(name));
    if (NS_FAILED(rv)) 
      return rv;
    
    actualContractID = NS_IMIME_MISC_STATUS_KEY;
    actualContractID.Append(name);
    
    // now we've got the CONTRACTID, let's add it to the list...
    newInfo = (miscStatusType *)PR_NEWZAP(miscStatusType);
    if (newInfo)
    {
      newInfo->obj = GetStatusObjForContractID(actualContractID);
      if (newInfo->obj)
      {
        newInfo->contractID.AssignWithConversion(actualContractID);
        mMiscStatusArray->AppendElement(newInfo);
      }
    }

    rv = components->Next();
  }
  
  return NS_OK;
}

nsIMimeMiscStatus *
nsMimeXULEmitter::GetStatusObjForContractID(nsCString aContractID)
{
  nsresult            rv = NS_OK;
  nsISupports         *obj = nsnull;

  nsCOMPtr<nsIComponentManager> comMgr = 
           do_GetService(kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) 
    return nsnull;
  
  nsCID         cid;
  rv = comMgr->ContractIDToClassID(aContractID, &cid);
  if (NS_FAILED(rv))
    return nsnull;

  rv = comMgr->CreateInstance(cid, nsnull, NS_GET_IID(nsIMimeMiscStatus), (void**)&obj);  
  if (NS_FAILED(rv))
    return nsnull;
  else
    return (nsIMimeMiscStatus *)obj;
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
nsMimeXULEmitter::WriteXULHeader()
{
  UtilityWriteCRLF("<?xml version=\"1.0\"?>");

  UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://messenger/skin/messageBody.css\" type=\"text/css\"?>");

  // Make it look consistent...
  UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://global/skin/global.css\" type=\"text/css\"?>");


  // Now, the XUL window!
  UtilityWriteCRLF("<window ");
  UtilityWriteCRLF("xmlns:html=\"http://www.w3.org/1999/xhtml\" ");
  UtilityWriteCRLF("xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" ");
  UtilityWriteCRLF("xmlns=\"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul\" ");
  DoWindowStatusProcessing();
  UtilityWriteCRLF("align=\"vertical\" flex=\"1\"> ");

  // Now, the JavaScript...
  UtilityWriteCRLF("<html:script language=\"javascript\" src=\"chrome://messenger/content/attach.js\"/>");
  UtilityWriteCRLF("<html:script language=\"javascript\" src=\"chrome://messenger/content/mime.js\"/>");
  DoGlobalStatusProcessing();

  return NS_OK;
}

nsresult
nsMimeXULEmitter::DoSpecialSenderProcessing(const char *field, const char *value)
{
	nsresult rv = NS_OK;
	if (!nsCRT::strcmp(field, "From"))
	{
		nsCOMPtr<nsIAbAddressCollecter> addressCollecter = 
		         do_GetService(kCAddressCollecter, &rv);
		if (NS_SUCCEEDED(rv) && addressCollecter)
			addressCollecter->CollectAddress(value);
	}
	return rv;
}

//
// This finalizes the header field being parsed.
//
nsresult
nsMimeXULEmitter::EndHeader()
{
  //
  // If this is NOT the mail messages's header, then we need to write
  // out the contents of the headers in HTML form.
  //
  if (!mDocHeader)
  {
    WriteHTMLHeaders();
    return NS_OK;
  }

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
  // First the top of the document...
  WriteXULHeader();

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

//
// This will be called for every header field regardless if it is in an
// internal body or the outer message. If it is the 
//
NS_IMETHODIMP
nsMimeXULEmitter::AddHeaderField(const char *field, const char *value)
{
  if (mDocHeader)
    DoSpecialSenderProcessing(field, value);

  return nsMimeBaseEmitter::AddHeaderField(field, value);
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
        PR_FREEIF(escapedUrl);
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
        rv = messageUrl->GetUri(&urlString);
      if (NS_SUCCEEDED(rv) && urlString)
      {
        UtilityWrite(urlString);
        PR_FREEIF(urlString);
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
  nsresult    rv = NS_OK;

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
  char *newNameValue = nsEscape(name, url_XAlphas);
  if (newNameValue) 
  {
    newName.Assign(newNameValue);
    PR_FREEIF(newNameValue);
  }
  else
  {
    newName.Assign(name);
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
  // Now we will write out the XUL/IFRAME line for the mBody
  //
  UtilityWrite("<html:iframe id=\"mail-body-frame\" type=\"content-primary\" src=\"");
  UtilityWrite("data:text/html;base64,");
  char *encoded = PL_Base64Encode(mBody, 0, nsnull);
  if (!encoded)
    return NS_ERROR_OUT_OF_MEMORY;

  UtilityWrite(encoded);
  PR_Free(encoded);
  UtilityWriteCRLF("\" border=\"0\" scrolling=\"auto\" resize=\"yes\" width=\"100%\" flex=\"1\"/>");
  return NS_OK;
}

nsresult
nsMimeXULEmitter::OutputGenericHeader(const char *aHeaderVal)
{
  char      *val = GetHeaderValue(aHeaderVal, mHeaderArray);
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
      UtilityWriteCRLF("<spacer flex=\"2\"/>");

          // This is the envelope information
          OutputGenericHeader(HEADER_SUBJECT);
          OutputGenericHeader(HEADER_FROM);
          OutputGenericHeader(HEADER_DATE);

      UtilityWriteCRLF("<spacer flex=\"2\"/>");
      UtilityWriteCRLF("</box>");


      UtilityWriteCRLF("<box name=\"header-attachment\" align=\"horizontal\" flex=\"1\">");
      UtilityWriteCRLF("<spacer flex=\"1\"/>");

        // Now the addbook and attachment icons need to be displayed
        DumpAddBookIcon(GetHeaderValue(HEADER_FROM, mHeaderArray));
        DumpAttachmentMenu();

      UtilityWriteCRLF("<spacer flex=\"1\"/>");
      UtilityWriteCRLF("</box>");

  UtilityWriteCRLF("</box>");
  UtilityWriteCRLF("</toolbar>");

  return NS_OK;
}

nsresult
nsMimeXULEmitter::DumpToCC()
{
  char * toField = GetHeaderValue(HEADER_TO, mHeaderArray);
  char * ccField = GetHeaderValue(HEADER_CC, mHeaderArray);
  char * bccField = GetHeaderValue(HEADER_BCC, mHeaderArray);
  char * newsgroupField = GetHeaderValue(HEADER_NEWSGROUPS, mHeaderArray);

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
    char  *userAgent = nsMimeXULEmitter::GetHeaderValue(HEADER_USER_AGENT, mHeaderArray);
    
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
    UtilityWriteCRLF(newValue);
    PR_FREEIF(newValue);
  }
  else
  {
    UtilityWriteCRLF(value);
  }

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
      PR_FREEIF(newValue);
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
    PR_FREEIF(htmlString);
  }

  // Deal with extra quotes...
  workAddr.CompressSet("\"", nsnull);

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

  PR_FREEIF(link);
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
