/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "prlog.h"
#include "nsCOMPtr.h"
#include "modlmime.h"
#include "nsCRT.h"
#include "mimeobj.h"
#include "mimemsg.h"
#include "mimetric.h"   /* for MIME_RichtextConverter */
#include "mimethtm.h"
#include "mimemsig.h"
#include "mimemrel.h"
#include "mimemalt.h"
#include "mimebuf.h"
#include "mimemapl.h"
#include "prprf.h"
#include "mimei.h"      /* for moved MimeDisplayData struct */
#include "mimebuf.h"
#include "prmem.h"
#include "plstr.h"
#include "prmem.h"
#include "mimemoz2.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsFileSpec.h"
#include "comi18n.h"
#include "nsIStringBundle.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsMimeStringResources.h"
#include "nsStreamConverter.h"
#include "nsIMsgSend.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsSpecialSystemDirectory.h"
#include "mozITXTToHTMLConv.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsIImapUrl.h"
#include "nsMsgI18N.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsMimeTypes.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsIMsgWindow.h"
#include "nsMsgUtils.h"
#include "nsIChannel.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICacheSession.h"
#include "nsITransport.h"
#include "mimeebod.h"
#include "mimeeobj.h"
// <for functions="HTML2Plaintext,HTMLSantinize">
#include "nsXPCOM.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsIHTMLContentSink.h"
#include "nsIContentSerializer.h"
#include "nsLayoutCID.h"
#include "nsIComponentManager.h"
#include "nsReadableUtils.h"
#include "nsIHTMLToTextSink.h"
#include "mozISanitizingSerializer.h"
// </for>


static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
// <for functions="HTML2Plaintext,HTMLSantinize">
static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);
static NS_DEFINE_CID(kNavDTDCID, NS_CNAVDTD_CID);
// </for>

#ifdef HAVE_MIME_DATA_SLOT
#define LOCK_LAST_CACHED_MESSAGE
#endif

void                 ValidateRealName(nsMsgAttachmentData *aAttach, MimeHeaders *aHdrs);

static MimeHeadersState MIME_HeaderType;
static PRBool MIME_WrapLongLines;
static PRBool MIME_VariableWidthPlaintext;

// For string bundle access routines...
static nsCOMPtr<nsIStringBundle>   stringBundle = nsnull;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Attachment handling routines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
MimeObject    *mime_get_main_object(MimeObject* obj);

nsresult
ProcessBodyAsAttachment(MimeObject *obj, nsMsgAttachmentData **data)
{
  nsMsgAttachmentData   *tmp;
  PRInt32               n;
  char                  *disp = nsnull;
  char                  *charset = nsnull;

  // Ok, this is the special case when somebody sends an "attachment" as the body
  // of an RFC822 message...I really don't think this is the way this should be done.
  // I belive this should really be a multipart/mixed message with an empty body part,
  // but what can ya do...our friends to the North seem to do this.
  //
  MimeObject    *child = obj;

  n = 1;
  *data = (nsMsgAttachmentData *)PR_Malloc( (n + 1) * sizeof(nsMsgAttachmentData));
  if (!*data) 
    return NS_ERROR_OUT_OF_MEMORY;

  tmp = *data;
  memset(*data, 0, (n + 1) * sizeof(nsMsgAttachmentData));
  tmp->real_type = child->content_type ? nsCRT::strdup(child->content_type) : NULL;
  tmp->real_encoding = child->encoding ? nsCRT::strdup(child->encoding) : NULL;
  disp = MimeHeaders_get(child->headers, HEADER_CONTENT_DISPOSITION, PR_FALSE, PR_FALSE);
  tmp->real_name = MimeHeaders_get_parameter(disp, "name", &charset, NULL);
  if (tmp->real_name)
  {
    char *fname = NULL;
    fname = mime_decode_filename(tmp->real_name, charset, obj->options);
    nsMemory::Free(charset);
    if (fname && fname != tmp->real_name)
    {
      PR_Free(tmp->real_name);
      tmp->real_name = fname;
    }
  }
  else
  {
    tmp->real_name = MimeHeaders_get_name(child->headers, obj->options);
  }

  if ( (!tmp->real_name) && (tmp->real_type) && (nsCRT::strncasecmp(tmp->real_type, "text", 4)) )
    ValidateRealName(tmp, child->headers);

  char  *tmpURL = nsnull;
  char  *id = nsnull;
  char  *id_imap = nsnull;

  id = mime_part_address (obj);
  if (obj->options->missing_parts)
    id_imap = mime_imap_part_address (obj);

  if (! id)
  {
    PR_FREEIF(*data);
    PR_FREEIF(id_imap);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (obj->options && obj->options->url)
  {
    const char  *url = obj->options->url;
    nsresult    rv;
    if (id_imap && id)
    {
      // if this is an IMAP part. 
      tmpURL = mime_set_url_imap_part(url, id_imap, id);
      rv = nsMimeNewURI(&(tmp->url), tmpURL, nsnull);

      tmp->notDownloaded = PR_TRUE;
    }
    else
    {
      // This is just a normal MIME part as usual. 
      tmpURL = mime_set_url_part(url, id, PR_TRUE);
      rv = nsMimeNewURI(&(tmp->url), tmpURL, nsnull);
    }

    if ( (!tmp->url) || (NS_FAILED(rv)) )
    {
      PR_FREEIF(*data);
      PR_FREEIF(id);
      PR_FREEIF(id_imap);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  PR_FREEIF(id);
  PR_FREEIF(id_imap);
  PR_FREEIF(tmpURL);
  tmp->description = MimeHeaders_get(child->headers, HEADER_CONTENT_DESCRIPTION, PR_FALSE, PR_FALSE);
  return NS_OK;
}

PRInt32
CountTotalMimeAttachments(MimeContainer *aObj)
{
  PRInt32     i;
  PRInt32     rc = 0;

  if ( (!aObj) || (!aObj->children) || (aObj->nchildren <= 0) )
    return 0;

  if (mime_typep((MimeObject *)aObj, (MimeObjectClass *)&mimeExternalBodyClass))
    return 0;

  for (i=0; i<aObj->nchildren; i++)
    rc += CountTotalMimeAttachments((MimeContainer *)aObj->children[i]) + 1;

  return rc;
}

void
ValidateRealName(nsMsgAttachmentData *aAttach, MimeHeaders *aHdrs)
{ 
  // Sanity.
  if (!aAttach)
    return;

  // Do we need to validate?
  if ( (aAttach->real_name) && (*(aAttach->real_name)) )
    return;

  // Internal MIME structures need not be named!
  if ( (!aAttach->real_type) || (aAttach->real_type && 
                                 !nsCRT::strncasecmp(aAttach->real_type, "multipart", 9)) )
    return;

  // Special case...if this is a enclosed RFC822 message, give it a nice
  // name.
  if (aAttach->real_type && !nsCRT::strcasecmp(aAttach->real_type, MESSAGE_RFC822))
  {
    NS_ASSERTION(aHdrs, "How comes the object's headers is null!");
    if (aHdrs && aHdrs->munged_subject)
      aAttach->real_name = PR_smprintf("%s.eml", aHdrs->munged_subject);
    else
      NS_MsgSACopy(&(aAttach->real_name), "ForwardedMessage.eml");
    return;
  }

  // 
  // Now validate any other name we have for the attachment!
  //
  if (!aAttach->real_name || *aAttach->real_name == 0)
  {
    nsString  newAttachName(NS_LITERAL_STRING("attachment"));
    nsresult  rv = NS_OK;
    nsCAutoString contentType (aAttach->real_type);
    PRInt32 pos = contentType.FindChar(';');
    if (pos > 0)
      contentType.Truncate(pos);

    nsCOMPtr<nsIMIMEService> mimeFinder (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) 
    {
      nsCAutoString fileExtension;
      rv = mimeFinder->GetPrimaryExtension(contentType, EmptyCString(), fileExtension);

      if (NS_SUCCEEDED(rv) && !fileExtension.IsEmpty())
      {
        newAttachName.Append(PRUnichar('.'));
        AppendUTF8toUTF16(fileExtension, newAttachName);
      }
    }

    aAttach->real_name = ToNewCString(newAttachName);
  }  
}

static  PRInt32     attIndex = 0;

nsresult
GenerateAttachmentData(MimeObject *object, const char *aMessageURL, MimeDisplayOptions *options,
                       PRBool isAnAppleDoublePart, nsMsgAttachmentData *aAttachData)
{
  nsXPIDLCString imappart;
  nsXPIDLCString part;
  PRBool isIMAPPart;

  /* be sure the object has not be marked as Not to be an attachment */
  if (object->dontShowAsAttachment)
    return NS_OK;

  part.Adopt(mime_part_address(object));
  if (part.IsEmpty()) 
    return NS_ERROR_OUT_OF_MEMORY;

  if (options->missing_parts)
    imappart.Adopt(mime_imap_part_address(object));

  char *urlSpec = nsnull;
  if (!imappart.IsEmpty())
  {
    isIMAPPart = PR_TRUE;
    urlSpec = mime_set_url_imap_part(aMessageURL, imappart.get(), part.get());
  }
  else
  {
    isIMAPPart = PR_FALSE;
    char *no_part_url = nsnull;
    if (options->part_to_load && options->format_out == nsMimeOutput::nsMimeMessageBodyDisplay)
      no_part_url = mime_get_base_url(aMessageURL);
    if (no_part_url) {
      urlSpec = mime_set_url_part(no_part_url, part.get(), PR_TRUE);
      PR_Free(no_part_url);
    }
    else
      urlSpec = mime_set_url_part(aMessageURL, part.get(), PR_TRUE);
  }

  if (!urlSpec)
    return NS_ERROR_OUT_OF_MEMORY;

  if ((options->format_out == nsMimeOutput::nsMimeMessageBodyDisplay) && (nsCRT::strncasecmp(aMessageURL, urlSpec, strlen(urlSpec)) == 0))
    return NS_OK;
  nsMsgAttachmentData *tmp = &(aAttachData[attIndex++]);
  nsresult rv = nsMimeNewURI(&(tmp->url), urlSpec, nsnull);

	PR_FREEIF(urlSpec);

  if ( (NS_FAILED(rv)) || (!tmp->url) )
    return NS_ERROR_OUT_OF_MEMORY;

  tmp->real_type = object->content_type ? nsCRT::strdup(object->content_type) : nsnull;
  tmp->real_encoding = object->encoding ? nsCRT::strdup(object->encoding) : nsnull;
  
  PRInt32 i;
  char *charset = nsnull;
  char *disp = MimeHeaders_get(object->headers, HEADER_CONTENT_DISPOSITION, PR_FALSE, PR_FALSE);
  if (disp) 
  {
    tmp->real_name = MimeHeaders_get_parameter(disp, "filename", &charset, nsnull);
    if (isAnAppleDoublePart)
      for (i = 0; i < 2 && !tmp->real_name; i ++)
      {
        PR_FREEIF(disp);
        nsMemory::Free(charset);
        disp = MimeHeaders_get(((MimeContainer *)object)->children[i]->headers, HEADER_CONTENT_DISPOSITION, PR_FALSE, PR_FALSE);
        tmp->real_name = MimeHeaders_get_parameter(disp, "filename", &charset, nsnull);
      }

    if (tmp->real_name)
    {
      // check encoded type
      //
      // The parameter of Content-Disposition must use RFC 2231.
      // But old Netscape 4.x and Outlook Express etc. use RFC2047.
      // So we should parse both types.

      char *fname = nsnull;
      fname = mime_decode_filename(tmp->real_name, charset, options);
      nsMemory::Free(charset);

      if (fname && fname != tmp->real_name)
      {
        PR_FREEIF(tmp->real_name);
        tmp->real_name = fname;
      }
    }

    PR_FREEIF(disp);
  }

  disp = MimeHeaders_get(object->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
  if (disp)
  {
    tmp->x_mac_type   = MimeHeaders_get_parameter(disp, PARAM_X_MAC_TYPE, nsnull, nsnull);
    tmp->x_mac_creator= MimeHeaders_get_parameter(disp, PARAM_X_MAC_CREATOR, nsnull, nsnull);
    
    if (!tmp->real_name || *tmp->real_name == 0)
    {
      PR_FREEIF(tmp->real_name);
      tmp->real_name = MimeHeaders_get_parameter(disp, "name", &charset, nsnull);
      if (isAnAppleDoublePart)
        // the data fork is the 2nd part, and we should ALWAYS look there first for the file name
        for (i = 1; i >= 0 && !tmp->real_name; i --) 
        {
          PR_FREEIF(disp);
          nsMemory::Free(charset);
          disp = MimeHeaders_get(((MimeContainer *)object)->children[i]->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
          tmp->real_name = MimeHeaders_get_parameter(disp, "name", &charset, nsnull);
        }
      
      if (tmp->real_name)
      {
        // check encoded type
        //
        // The parameter of Content-Disposition must use RFC 2231.
        // But old Netscape 4.x and Outlook Express etc. use RFC2047.
        // So we should parse both types.

        char *fname = nsnull;
        fname = mime_decode_filename(tmp->real_name, charset, options);
        nsMemory::Free(charset);

        if (fname && fname != tmp->real_name)
        {
          PR_Free(tmp->real_name);
          tmp->real_name = fname;
        }
      }
    }
    PR_FREEIF(disp);
  }

  tmp->description = MimeHeaders_get(object->headers, HEADER_CONTENT_DESCRIPTION, 
                                     PR_FALSE, PR_FALSE);

  // Now, do the right thing with the name!
  if (!tmp->real_name && nsCRT::strcasecmp(tmp->real_type, MESSAGE_RFC822))
  {
    /* If this attachment doesn't have a name, just give it one... */
    tmp->real_name = MimeGetStringByID(MIME_MSG_DEFAULT_ATTACHMENT_NAME);
    if (tmp->real_name)
    {
      char *newName = PR_smprintf(tmp->real_name, part.get());
      if (newName)
      {
        PR_Free(tmp->real_name);
        tmp->real_name = newName;
      }
    }
    else
      tmp->real_name = mime_part_address(object);
  }
  ValidateRealName(tmp, object->headers);

  if (isIMAPPart)
  {
    // If we get here, we should mark this attachment as not being
    // downloaded. 
    tmp->notDownloaded = PR_TRUE;
  }

  return NS_OK;
}

nsresult
BuildAttachmentList(MimeObject *anObject, nsMsgAttachmentData *aAttachData, const char *aMessageURL)
{
  nsresult              rv;
  PRInt32               i;
  MimeContainer         *cobj = (MimeContainer *) anObject;

  if ( (!anObject) || (!cobj->children) || (!cobj->nchildren) ||
       (mime_typep(anObject, (MimeObjectClass *)&mimeExternalBodyClass)))
    return NS_OK;

  for (i = 0; i < cobj->nchildren ; i++) 
  {
    MimeObject    *child = cobj->children[i];

    // Skip the first child if it's in fact a message body
    if (i == 0)                                         // it's the first child
      if (child->content_type)                          // and it's content-type is one of folowing...
        if (!nsCRT::strcasecmp (child->content_type, TEXT_PLAIN) ||
            !nsCRT::strcasecmp (child->content_type, TEXT_HTML) ||
            !nsCRT::strcasecmp (child->content_type, TEXT_MDL))
        {
          if (child->headers) // and finally, be sure it doesn't have a content-disposition: attachment 
          {
            char * disp = MimeHeaders_get (child->headers, HEADER_CONTENT_DISPOSITION, PR_TRUE, PR_FALSE);
            if (!disp || nsCRT::strcasecmp (disp, "attachment"))
              continue;
          }
          else
            continue;
        }

    
    // We should generate an attachment for leaf object only but...
    PRBool isALeafObject = mime_subclass_p(child->clazz, (MimeObjectClass *) &mimeLeafClass);

    // ...we will generate an attachment for inline message too.
    PRBool isAnInlineMessage = mime_typep(child, (MimeObjectClass *) &mimeMessageClass);
    
    // AppleDouble part need special care: we need to fetch the part as well its two
    // children for the needed info as they could be anywhere, eventually, they won't contain
    // a name or file name. In any case we need to build only one attachment data
    PRBool isAnAppleDoublePart = mime_typep(child, (MimeObjectClass *) &mimeMultipartAppleDoubleClass) &&
                                 ((MimeContainer *)child)->nchildren == 2;

    if (isALeafObject || isAnInlineMessage || isAnAppleDoublePart)
    {
      rv = GenerateAttachmentData(child, aMessageURL, anObject->options, isAnAppleDoublePart, aAttachData);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    
    // Now build the attachment list for the children of our object...
    if (!isALeafObject && !isAnAppleDoublePart)
    {
      rv = BuildAttachmentList((MimeObject *)child, aAttachData, aMessageURL);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;

}

extern "C" nsresult
MimeGetAttachmentList(MimeObject *tobj, const char *aMessageURL, nsMsgAttachmentData **data)
{
  MimeObject            *obj;
  MimeContainer         *cobj;
  PRInt32               n;
  PRBool                isAnInlineMessage;

  if (!data) 
    return 0;
  *data = nsnull;

  obj = mime_get_main_object(tobj);
  if (!obj)
    return 0;

  if (!mime_subclass_p(obj->clazz, (MimeObjectClass*) &mimeContainerClass))
  {
    if (!PL_strcasecmp(obj->content_type, MESSAGE_RFC822))
      return 0;
    else
      return ProcessBodyAsAttachment(obj, data);
  }

  isAnInlineMessage = mime_typep(obj, (MimeObjectClass *) &mimeMessageClass);

  cobj = (MimeContainer*) obj;
  n = CountTotalMimeAttachments(cobj);
  if (n <= 0) 
    return n;

  //in case of an inline message (as body), we need an extra slot for the message itself
  //that we will fill later...
  if (isAnInlineMessage)
    n ++;

  *data = (nsMsgAttachmentData *)PR_Malloc( (n + 1) * sizeof(nsMsgAttachmentData));
  if (!*data) 
    return NS_ERROR_OUT_OF_MEMORY;

  attIndex = 0;
  memset(*data, 0, (n + 1) * sizeof(nsMsgAttachmentData));
  
  // Now, build the list!

  nsresult rv;

  if (isAnInlineMessage)
  {
    rv = GenerateAttachmentData(obj, aMessageURL, obj->options, PR_FALSE, *data);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return BuildAttachmentList((MimeObject *) cobj, *data, aMessageURL);
}

extern "C" void
MimeFreeAttachmentList(nsMsgAttachmentData *data)
{
  if (data) 
  {
    nsMsgAttachmentData   *tmp;
    for (tmp = data ; tmp->url ; tmp++) 
    {
      /* Can't do PR_FREEIF on `const' values... */
      NS_IF_RELEASE(tmp->url);
      if (tmp->real_type) PR_Free((char *) tmp->real_type);
      if (tmp->real_encoding) PR_Free((char *) tmp->real_encoding);
      if (tmp->real_name) PR_Free((char *) tmp->real_name);
      if (tmp->x_mac_type) PR_Free((char *) tmp->x_mac_type);
      if (tmp->x_mac_creator) PR_Free((char *) tmp->x_mac_creator);
      if (tmp->description) PR_Free((char *) tmp->description);
      tmp->url = 0;
      tmp->real_type = 0;
      tmp->real_name = 0;
      tmp->description = 0;
    }
    PR_Free(data);
  }
}

extern "C" void
NotifyEmittersOfAttachmentList(MimeDisplayOptions     *opt,
                               nsMsgAttachmentData    *data)
{
  PRInt32     i = 0;
  struct      nsMsgAttachmentData  *tmp = data;

  if (!tmp) 
    return;

  while (tmp->url)
  {
    if (!tmp->real_name)
    {
      ++i;
      ++tmp;      
      continue;
    }

    nsCAutoString spec;
    if ( tmp->url ) 
      tmp->url->GetSpec(spec);

    mimeEmitterStartAttachment(opt, tmp->real_name, tmp->real_type, spec.get(), tmp->notDownloaded);
    mimeEmitterAddAttachmentField(opt, HEADER_X_MOZILLA_PART_URL, spec.get());

    if ( (opt->format_out == nsMimeOutput::nsMimeMessageQuoting) || 
         (opt->format_out == nsMimeOutput::nsMimeMessageBodyQuoting) || 
         (opt->format_out == nsMimeOutput::nsMimeMessageSaveAs) || 
         (opt->format_out == nsMimeOutput::nsMimeMessagePrintOutput) )
    {
      mimeEmitterAddAttachmentField(opt, HEADER_CONTENT_DESCRIPTION, tmp->description);
      mimeEmitterAddAttachmentField(opt, HEADER_CONTENT_TYPE, tmp->real_type);
      mimeEmitterAddAttachmentField(opt, HEADER_CONTENT_ENCODING,    tmp->real_encoding);

      /* rhp - for now, just leave these here, but they are really
               not necessary
      printf("URL for Part      : %s\n", spec);
      printf("Real Name         : %s\n", tmp->real_name);
	    printf("Desired Type      : %s\n", tmp->desired_type);
      printf("Real Type         : %s\n", tmp->real_type);
	    printf("Real Encoding     : %s\n", tmp->real_encoding); 
      printf("Description       : %s\n", tmp->description);
      printf("Mac Type          : %s\n", tmp->x_mac_type);
      printf("Mac Creator       : %s\n", tmp->x_mac_creator);
      */
    }

    mimeEmitterEndAttachment(opt);
    ++i;
    ++tmp;
  }
  mimeEmitterEndAllAttachments(opt);
}

// Utility to create a nsIURI object...
extern "C" nsresult 
nsMimeNewURI(nsIURI** aInstancePtrResult, const char *aSpec, nsIURI *aBase)
{  
  nsresult  res;

  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIIOService> pService(do_GetService(kIOServiceCID, &res));
  if (NS_FAILED(res)) 
    return NS_ERROR_FACTORY_NOT_REGISTERED;

  return pService->NewURI(nsDependentCString(aSpec), nsnull, aBase, aInstancePtrResult);
}

extern "C" nsresult 
SetMailCharacterSetToMsgWindow(MimeObject *obj, const char *aCharacterSet)
{
  nsresult rv = NS_OK;

  if (obj && obj->options)
  {
    mime_stream_data *msd = (mime_stream_data *) (obj->options->stream_closure);
    if (msd)
    {
      nsIChannel *channel = msd->channel;
      if (channel)
      {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri)
        {
          nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(uri));
          if (msgurl)
          {
            nsCOMPtr<nsIMsgWindow> msgWindow;
            msgurl->GetMsgWindow(getter_AddRefs(msgWindow));
            if (msgWindow)
              rv = msgWindow->SetMailCharacterSet(!nsCRT::strcasecmp(aCharacterSet, "us-ascii") ?
                                                  "ISO-8859-1" :
                                                  aCharacterSet);
          }
        }
      }
    }
  }

  return rv;
}

static char *
mime_file_type (const char *filename, void *stream_closure)
{
  char        *retType = nsnull;
  char        *ext = nsnull;
  nsresult    rv;

  ext = PL_strrchr(filename, '.');
  if (ext)
  {
    ext++;
    nsCOMPtr<nsIMIMEService> mimeFinder (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    if (mimeFinder) {
      nsCAutoString type;
      mimeFinder->GetTypeFromExtension(nsDependentCString(ext), type);
      retType = ToNewCString(type);
    }
  }

  return retType;
}

int ConvertUsingEncoderAndDecoder(const char *stringToUse, PRInt32 inLength, 
                                  nsIUnicodeEncoder *encoder, nsIUnicodeDecoder *decoder, 
                                  char **pConvertedString, PRInt32 *outLength)
{
  // buffer size 144 =
  // 72 (default line len for compose) 
  // times 2 (converted byte len might be larger)
  const int klocalbufsize = 144;
  // do the conversion
  PRUnichar *unichars;
  PRInt32 unicharLength;
  PRInt32 srcLen = inLength;
  PRInt32 dstLength = 0;
  char *dstPtr;
  nsresult rv;

  // use this local buffer if possible
  PRUnichar localbuf[klocalbufsize+1];
  if (inLength > klocalbufsize) {
    rv = decoder->GetMaxLength(stringToUse, srcLen, &unicharLength);
    // allocate temporary buffer to hold unicode string
    unichars = new PRUnichar[unicharLength];
  }
  else {
    unichars = localbuf;
    unicharLength = klocalbufsize+1;
  }
  if (unichars == nsnull) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    // convert to unicode, replacing failed chars with 0xFFFD as in
    // the methode used in nsXMLHttpRequest::ConvertBodyToText and nsScanner::Append
    // 
    // We will need several pass to convert the whole string if it has invalid characters
    // 'totalChars' is where the sum of the number of converted characters will be done
    // 'dataLen' is the number of character left to convert
    // 'outLen' is the number of characters still available in the output buffer as input of decoder->Convert
    // and the number of characters written in it as output.
    PRInt32 totalChars = 0,
            inBufferIndex = 0,
            outBufferIndex = 0;
    PRInt32 dataLen = srcLen,
            outLen = unicharLength;

    do {
      PRInt32 inBufferLength = dataLen;
      rv = decoder->Convert(&stringToUse[inBufferIndex],
                           &inBufferLength,
                           &unichars[outBufferIndex],
                           &outLen);
      totalChars += outLen;
      // Done if conversion successful
      if (NS_SUCCEEDED(rv))
          break;

      // We consume one byte, replace it with U+FFFD
      // and try the conversion again.
      outBufferIndex += outLen;
      unichars[outBufferIndex++] = PRUnichar(0xFFFD);
      // totalChars is updated here
      outLen = unicharLength - (++totalChars);

      inBufferIndex += inBufferLength + 1;
      dataLen -= inBufferLength + 1;

      decoder->Reset();

      // If there is not at least one byte available after the one we
      // consumed, we're done
    } while ( dataLen > 0 );

    rv = encoder->GetMaxLength(unichars, totalChars, &dstLength);
    // allocale an output buffer
    dstPtr = (char *) PR_Malloc(dstLength + 1);
    if (dstPtr == nsnull) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else {
      PRInt32 buffLength = dstLength;
      // convert from unicode
      rv = encoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, '?');
      if (NS_SUCCEEDED(rv)) {
        rv = encoder->Convert(unichars, &totalChars, dstPtr, &dstLength);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 finLen = buffLength - dstLength;
          rv = encoder->Finish((char *)(dstPtr+dstLength), &finLen);
          if (NS_SUCCEEDED(rv)) {
            dstLength += finLen;
          }
          dstPtr[dstLength] = '\0';
          *pConvertedString = dstPtr;       // set the result string
          *outLength = dstLength;
        }
      }
    }
    if (inLength > klocalbufsize)
      delete [] unichars;
  }

  return NS_SUCCEEDED(rv) ? 0 : -1;
}


static int
mime_convert_charset (const char *input_line, PRInt32 input_length,
                      const char *input_charset, const char *output_charset,
                      char **output_ret, PRInt32 *output_size_ret,
                      void *stream_closure, nsIUnicodeDecoder *decoder, nsIUnicodeEncoder *encoder)
{
  PRInt32 res = -1;
  char  *convertedString = NULL;
  PRInt32 convertedStringLen = 0;
  if (encoder && decoder)
  {
    res = ConvertUsingEncoderAndDecoder(input_line, input_length, encoder, decoder, &convertedString, &convertedStringLen);
  }
  if (res != 0)
  {
      *output_ret = 0;
      *output_size_ret = 0;
  }
  else
  {
    *output_ret = (char *) convertedString;
    *output_size_ret = convertedStringLen;
  }  

  return 0;
}

static int
mime_output_fn(const char *buf, PRInt32 size, void *stream_closure)
{
  PRUint32  written = 0;
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  if ( (!msd->pluginObj2) && (!msd->output_emitter) )
    return -1;
  
  // Fire pending start request
  ((nsStreamConverter*)msd->pluginObj2)->FirePendingStartRequest();
  
  
  // Now, write to the WriteBody method if this is a message body and not
  // a part retrevial
  if (!msd->options->part_to_load || msd->options->format_out == nsMimeOutput::nsMimeMessageBodyDisplay)
  {
    if (msd->output_emitter)
    {
      msd->output_emitter->WriteBody(buf, (PRUint32) size, &written);
    }
  }
  else
  {
    if (msd->output_emitter)
    {
      msd->output_emitter->Write(buf, (PRUint32) size, &written);
    }
  }
  return written;
}

#ifdef XP_MAC
static int
compose_only_output_fn(const char *buf, PRInt32 size, void *stream_closure)
{
    return 0;
}
#endif

extern "C" int
mime_display_stream_write (nsMIMESession *stream,
                           const char* buf,
                           PRInt32 size)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) ((nsMIMESession *)stream)->data_object;

  MimeObject *obj = (msd ? msd->obj : 0);  
  if (!obj) return -1;

  //
  // Ok, now check to see if this is a display operation for a MIME Parts on Demand
  // enabled call.
  //
  if (msd->firstCheck)
  {
    if (msd->channel)
    {
      nsCOMPtr<nsIURI> aUri;
	    if (NS_SUCCEEDED(msd->channel->GetURI(getter_AddRefs(aUri))))
      {
        nsCOMPtr<nsIImapUrl> imapURL = do_QueryInterface(aUri);
        if (imapURL)
        {
          nsImapContentModifiedType   cModified;
          if (NS_SUCCEEDED(imapURL->GetContentModified(&cModified)))
          {
            if ( cModified != nsImapContentModifiedTypes::IMAP_CONTENT_NOT_MODIFIED )
              msd->options->missing_parts = PR_TRUE;
          }
        }
      }
    }

    msd->firstCheck = PR_FALSE;
  }

  return obj->clazz->parse_buffer((char *) buf, size, obj);
}

extern "C" void 
mime_display_stream_complete (nsMIMESession *stream)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) ((nsMIMESession *)stream)->data_object;
  MimeObject *obj = (msd ? msd->obj : 0);  
  if (obj)
  {
    int       status;
    PRBool    abortNow = PR_FALSE;

    // Release the prefs service
    if ( (obj->options) && (obj->options->prefs) )
      nsServiceManager::ReleaseService(kPrefCID, obj->options->prefs);
    
    if ((obj->options) && (obj->options->headers == MimeHeadersOnly))
      abortNow = PR_TRUE;

    status = obj->clazz->parse_eof(obj, abortNow);
    obj->clazz->parse_end(obj, (status < 0 ? PR_TRUE : PR_FALSE));
  
    //
    // Ok, now we are going to process the attachment data by getting all
    // of the attachment info and then driving the emitter with this data.
    //
    if (!msd->options->part_to_load || msd->options->format_out == nsMimeOutput::nsMimeMessageBodyDisplay)
    {
      nsMsgAttachmentData *attachments;
      nsresult rv = MimeGetAttachmentList(obj, msd->url_name, &attachments);
      if (NS_SUCCEEDED(rv))
      {
        NotifyEmittersOfAttachmentList(msd->options, attachments);
        MimeFreeAttachmentList(attachments);
      }
    }

    // Release the conversion object - this has to be done after
    // we finish processing data.
    if ( obj->options)
    {
      NS_IF_RELEASE(obj->options->conv);
    }

    // Destroy the object now.
    PR_ASSERT(msd->options == obj->options);
    mime_free(obj);
    obj = NULL;
    if (msd->options)
    {
      delete msd->options;
      msd->options = 0;
    }
  }

  if (msd->headers)
  	MimeHeaders_free (msd->headers);

  if (msd->url_name)
	  nsCRT::free(msd->url_name);

  if (msd->orig_url_name)
      nsCRT::free(msd->orig_url_name);

  PR_FREEIF(msd);
}

extern "C" void
mime_display_stream_abort (nsMIMESession *stream, int status)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) ((nsMIMESession *)stream)->data_object;
  
  MimeObject *obj = (msd ? msd->obj : 0);  
  if (obj)
  {
    if (!obj->closed_p)
      obj->clazz->parse_eof(obj, PR_TRUE);
    if (!obj->parsed_p)
      obj->clazz->parse_end(obj, PR_TRUE);
    
    // Destroy code....
    PR_ASSERT(msd->options == obj->options);
    mime_free(obj);
    if (msd->options)
    {
      delete msd->options;
      msd->options = 0;
    }
  }

  if (msd->headers)
  	MimeHeaders_free (msd->headers);

  if (msd->url_name)
	  nsCRT::free(msd->url_name);

  if (msd->orig_url_name)
      nsCRT::free(msd->orig_url_name);

  PR_FREEIF(msd);
}

#ifdef XP_MAC
static PRUint32
mime_convert_chars_to_ostype(const char *osTypeStr)
{
	if (!osTypeStr)
		return '????';

	PRUint32 result;
	const char *p = osTypeStr;

	for (result = 0; *p; p++)
	{
		char C = *p;

		PRInt8 unhex = ((C >= '0' && C <= '9') ? C - '0' :
			((C >= 'A' && C <= 'F') ? C - 'A' + 10 :
			 ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : -1)));
		if (unhex < 0)
			break;
		result = (result << 4) | unhex;
	}

	return result;
}
#endif

static int
mime_output_init_fn (const char *type,
                     const char *charset,
                     const char *name,
                     const char *x_mac_type,
                     const char *x_mac_creator,
                     void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  
  // Now, all of this stream creation is done outside of libmime, so this
  // is just a check of the pluginObj member and returning accordingly.
  if (!msd->pluginObj2)
    return -1;
  else
    return 0;
}

static void   *mime_image_begin(const char *image_url, const char *content_type,
                              void *stream_closure);
static void   mime_image_end(void *image_closure, int status);
static char   *mime_image_make_image_html(void *image_data);
static int    mime_image_write_buffer(const char *buf, PRInt32 size, void *image_closure);

/* Interface between libmime and inline display of images: the abomination
   that is known as "internal-external-reconnect".
 */
class mime_image_stream_data {
public:
  mime_image_stream_data();

  struct mime_stream_data *msd;
  char                    *url;
  nsMIMESession           *istream;
  nsCOMPtr<nsIOutputStream> memCacheOutputStream;
  PRBool m_shouldCacheImage;
};

mime_image_stream_data::mime_image_stream_data()
{
  url = nsnull;
  istream = nsnull;
  msd = nsnull;
  m_shouldCacheImage = PR_FALSE;
}

static void *
mime_image_begin(const char *image_url, const char *content_type,
                 void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  class mime_image_stream_data *mid;
 
  mid = new mime_image_stream_data;
  if (!mid) return nsnull;


  mid->msd = msd;

  mid->url = (char *) nsCRT::strdup(image_url);
  if (!mid->url)
  {
    PR_Free(mid);
    return nsnull;
  }

  if (msd->channel)
  {
    nsCOMPtr <nsIURI> uri;
    nsresult rv = msd->channel->GetURI(getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv) && uri)
    {
      nsCOMPtr <nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(uri);
      if (mailUrl)
      {
        nsCOMPtr<nsICacheSession> memCacheSession;
        mailUrl->GetImageCacheSession(getter_AddRefs(memCacheSession));
        if (memCacheSession)
        {
          nsCOMPtr<nsICacheEntryDescriptor> entry;

          // we may need to convert the image_url into just a part url - in any case,
          // it has to be the same as what imglib will be asking imap for later
          // on so that we'll find this in the memory cache.
          rv = memCacheSession->OpenCacheEntry(image_url, nsICache::ACCESS_READ_WRITE, nsICache::BLOCKING, getter_AddRefs(entry));
          nsCacheAccessMode access;
          if (entry)
          {
            entry->GetAccessGranted(&access);
#ifdef DEBUG_bienvenu
            printf("Mime opening cache entry for %s access = %ld\n", image_url, access);
#endif
            // if we only got write access, then we should fill in this cache entry
            if (access & nsICache::ACCESS_WRITE && !(access & nsICache::ACCESS_READ))
            {
              mailUrl->CacheCacheEntry(entry);
              entry->MarkValid();

              // remember the content type as meta data so we can pull it out in the imap code
              // to feed the cache entry directly to imglib w/o going through mime.
              entry->SetMetaDataElement("contentType", content_type);

              rv = entry->OpenOutputStream(0, getter_AddRefs(mid->memCacheOutputStream));
              if (NS_FAILED(rv)) return nsnull;
            }
          }
        }
      }
    }
  }
  mid->istream = (nsMIMESession *) msd->pluginObj2;
  return mid;
}

static void
mime_image_end(void *image_closure, int status)
{
  mime_image_stream_data *mid =
    (mime_image_stream_data *) image_closure;
  
  PR_ASSERT(mid);
  if (!mid) 
    return;
  if (mid->memCacheOutputStream)
    mid->memCacheOutputStream->Close();

  PR_FREEIF(mid->url);
  delete mid;
}


static char *
mime_image_make_image_html(void *image_closure)
{
  mime_image_stream_data *mid =
    (mime_image_stream_data *) image_closure;

  const char *prefix = "<P><CENTER><IMG SRC=\"";
  const char *suffix = "\"></CENTER><P>";
  const char *url;
  char *buf;

  PR_ASSERT(mid);
  if (!mid) return 0;

  /* Internal-external-reconnect only works when going to the screen. */
  if (!mid->istream)
    return nsCRT::strdup("<P><CENTER><IMG SRC=\"resource://gre/res/network/gopher-image.gif\" ALT=\"[Image]\"></CENTER><P>");

  if ( (!mid->url) || (!(*mid->url)) )
    url = "";
  else
    url = mid->url;

  buf = (char *) PR_MALLOC (strlen(prefix) + strlen(suffix) +
                            strlen(url) + 20) ;
  if (!buf) 
    return 0;
  *buf = 0;

  PL_strcat (buf, prefix);
  PL_strcat (buf, url);
  PL_strcat (buf, suffix);
  return buf;
}

static int
mime_image_write_buffer(const char *buf, PRInt32 size, void *image_closure)
{
  mime_image_stream_data *mid =
                (mime_image_stream_data *) image_closure;
  struct mime_stream_data *msd = mid->msd;

  if ( ( (!msd->output_emitter) ) &&
       ( (!msd->pluginObj2)     ) )
    return -1;

  //
  // If we get here, we are just eating the data this time around
  // and the returned URL will deal with writing the data to the viewer.
  // Just return the size value to the caller.
  //
  if (mid->memCacheOutputStream)
  {
    PRUint32 bytesWritten;
    mid->memCacheOutputStream->Write(buf, size, &bytesWritten);
  }
  return size;
}

MimeObject*
mime_get_main_object(MimeObject* obj)
{
  MimeContainer *cobj;
  if (!(mime_subclass_p(obj->clazz, (MimeObjectClass*) &mimeMessageClass))) 
  {
    return obj;
  }
  cobj = (MimeContainer*) obj;
  if (cobj->nchildren != 1) return obj;
  obj = cobj->children[0];
  while (obj)
  {
    if ( (!mime_subclass_p(obj->clazz,
         (MimeObjectClass*) &mimeMultipartSignedClass)) &&
         (PL_strcasecmp(obj->content_type, MULTIPART_SIGNED) != 0)
       )
    {
        return obj;
    }
    else
    {
      if (mime_subclass_p(obj->clazz, (MimeObjectClass*)&mimeContainerClass))
      {
        // We don't care about a signed/smime object; Go inside to the 
        // thing that we signed or smime'ed
        //
        cobj = (MimeContainer*) obj;
        if (cobj->nchildren > 0)
          obj = cobj->children[0];
        else
          obj = nsnull;
      }
      else
      {
        // we received a message with a child object that looks like a signed
        // object, but it is not a subclass of mimeContainer, so let's
        // return the given child object.
        return obj;
      }
    }
  }
  return nsnull;
}

PRBool MimeObjectChildIsMessageBody(MimeObject *obj, 
									 PRBool *isAlternativeOrRelated)
{
	char *disp = 0;
	PRBool bRet = PR_FALSE;
	MimeObject *firstChild = 0;
	MimeContainer *container = (MimeContainer*) obj;

	if (isAlternativeOrRelated)
		*isAlternativeOrRelated = PR_FALSE;

	if (!container ||
		!mime_subclass_p(obj->clazz, 
						 (MimeObjectClass*) &mimeContainerClass))
	{
		return bRet;
	}
	else if (mime_subclass_p(obj->clazz, (MimeObjectClass*)
							 &mimeMultipartRelatedClass)) 
	{
		if (isAlternativeOrRelated)
			*isAlternativeOrRelated = PR_TRUE;
		return bRet;
	}
	else if (mime_subclass_p(obj->clazz, (MimeObjectClass*)
							 &mimeMultipartAlternativeClass))
	{
		if (isAlternativeOrRelated)
			*isAlternativeOrRelated = PR_TRUE;
		return bRet;
	}

	if (container->children)
		firstChild = container->children[0];
	
	if (!firstChild || 
		!firstChild->content_type || 
		!firstChild->headers)
		return bRet;

	disp = MimeHeaders_get (firstChild->headers,
							HEADER_CONTENT_DISPOSITION, 
							PR_TRUE,
							PR_FALSE);
	if (disp /* && !nsCRT::strcasecmp (disp, "attachment") */)
		bRet = PR_FALSE;
	else if (!nsCRT::strcasecmp (firstChild->content_type, TEXT_PLAIN) ||
			 !nsCRT::strcasecmp (firstChild->content_type, TEXT_HTML) ||
			 !nsCRT::strcasecmp (firstChild->content_type, TEXT_MDL) ||
			 !nsCRT::strcasecmp (firstChild->content_type, MULTIPART_ALTERNATIVE) ||
			 !nsCRT::strcasecmp (firstChild->content_type, MULTIPART_RELATED) ||
       !nsCRT::strcasecmp (firstChild->content_type, MESSAGE_NEWS) ||
       !nsCRT::strcasecmp (firstChild->content_type, MESSAGE_RFC822))
		bRet = PR_TRUE;
	else
		bRet = PR_FALSE;
	PR_FREEIF(disp);
	return bRet;
}

//
// New Stream Converter Interface
//

// Get the connnection to prefs service manager 
nsIPref *
GetPrefServiceManager(MimeDisplayOptions *opt)
{
  if (!opt) 
    return nsnull;

  return opt->prefs;
}

// Get the text converter...
mozITXTToHTMLConv *
GetTextConverter(MimeDisplayOptions *opt)
{
  if (!opt) 
    return nsnull;

  return opt->conv;
}

MimeDisplayOptions::MimeDisplayOptions()
{
  conv = nsnull;        // For text conversion...
  prefs = nsnull;       /* Connnection to prefs service manager */
  format_out = 0;   // The format out type
  url = nsnull;	

  memset(&headers,0, sizeof(headers));	
  fancy_headers_p = PR_FALSE;

  output_vcard_buttons_p = PR_FALSE;

  fancy_links_p = PR_FALSE;

  variable_width_plaintext_p = PR_FALSE;
  wrap_long_lines_p = PR_FALSE;
  rot13_p = PR_FALSE;
  part_to_load = nsnull;

  write_html_p = PR_FALSE;

  decrypt_p = PR_FALSE;

  nice_html_only_p = PR_FALSE;

  whattodo = 0 ;
  default_charset = nsnull;
  override_charset = PR_FALSE;
  force_user_charset = PR_FALSE;
  stream_closure = nsnull;

  /* For setting up the display stream, so that the MIME parser can inform
	 the caller of the type of the data it will be getting. */
  output_init_fn = nsnull;
  output_fn = nsnull;

  output_closure = nsnull;

  charset_conversion_fn = nsnull;
  rfc1522_conversion_p = PR_FALSE;

  file_type_fn = nsnull;

  passwd_prompt_fn = nsnull;

  html_closure = nsnull;

  generate_header_html_fn = nsnull;
  generate_post_header_html_fn = nsnull;
  generate_footer_html_fn = nsnull;
  generate_reference_url_fn = nsnull;
  generate_mailto_url_fn = nsnull;
  generate_news_url_fn = nsnull;

  image_begin = nsnull;
  image_end = nsnull;
  image_write_buffer = nsnull;
  make_image_html = nsnull;
  state = nsnull;

#ifdef MIME_DRAFTS
  decompose_file_p = PR_FALSE;
  done_parsing_outer_headers = PR_FALSE;
  is_multipart_msg = PR_FALSE;
  decompose_init_count = 0;

  signed_p = PR_FALSE;
  caller_need_root_headers = PR_FALSE; 
  decompose_headers_info_fn = nsnull;
  decompose_file_init_fn = nsnull;
  decompose_file_output_fn = nsnull;
  decompose_file_close_fn = nsnull;
#endif /* MIME_DRAFTS */

  attachment_icon_layer_id = 0;

  missing_parts = PR_FALSE;
  show_attachment_inline_p = PR_FALSE;
}

MimeDisplayOptions::~MimeDisplayOptions()
{
  PR_FREEIF(part_to_load);
  PR_FREEIF(default_charset);
}
////////////////////////////////////////////////////////////////
// Bridge routines for new stream converter XP-COM interface 
////////////////////////////////////////////////////////////////
extern "C" void  *
mime_bridge_create_display_stream(
                          nsIMimeEmitter      *newEmitter,
                          nsStreamConverter   *newPluginObj2,
                          nsIURI              *uri,
                          nsMimeOutputType    format_out,
                          PRUint32	          whattodo,
                          nsIChannel          *aChannel)
{
  int                       status = 0;
  MimeObject                *obj;
  struct mime_stream_data   *msd;
  nsMIMESession             *stream = 0;
  
  if (!uri)
    return nsnull;

  msd = PR_NEWZAP(struct mime_stream_data);
  if (!msd) 
    return NULL;

  // Assign the new mime emitter - will handle output operations
  msd->output_emitter = newEmitter;
  msd->firstCheck = PR_TRUE;

  // Store the URL string for this decode operation
  nsCAutoString urlString;
  nsresult rv;

  // Keep a hold of the channel...
  msd->channel = aChannel;
  rv = uri->GetSpec(urlString);
  if (NS_SUCCEEDED(rv))
  {
    if (!urlString.IsEmpty())
    {
      msd->url_name = ToNewCString(urlString);
      if (!(msd->url_name))
      {
        PR_FREEIF(msd);
        return NULL;
      }
      nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(uri);
      if (msgUrl)
          msgUrl->GetOriginalSpec(&msd->orig_url_name);
    }
  }
  
  msd->format_out = format_out;       // output format
  msd->pluginObj2 = newPluginObj2;    // the plugin object pointer 
  
  msd->options = new MimeDisplayOptions;
  if (!msd->options)
  {
    PR_Free(msd);
    return 0;
  }
//  memset(msd->options, 0, sizeof(*msd->options));
  msd->options->format_out = format_out;     // output format

  rv = nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), (nsISupports**)&(msd->options->prefs));
  if (! (msd->options->prefs && NS_SUCCEEDED(rv)))
	{
    PR_FREEIF(msd);
    return nsnull;
  }

  // Need the text converter...
  rv = nsComponentManager::CreateInstance(MOZ_TXTTOHTMLCONV_CONTRACTID,
                                          NULL, NS_GET_IID(mozITXTToHTMLConv),
                                          (void **)&(msd->options->conv));
  if (NS_FAILED(rv))
	{
    nsServiceManager::ReleaseService(kPrefCID, msd->options->prefs);
    PR_FREEIF(msd);
    return nsnull;
  }

  //
  // Set the defaults, based on the context, and the output-type.
  //
  MIME_HeaderType = MimeHeadersAll;
  msd->options->write_html_p = PR_TRUE;
  switch (format_out) 
  {
		case nsMimeOutput::nsMimeMessageSplitDisplay:   // the wrapper HTML output to produce the split header/body display
		case nsMimeOutput::nsMimeMessageHeaderDisplay:  // the split header/body display
		case nsMimeOutput::nsMimeMessageBodyDisplay:    // the split header/body display
      msd->options->fancy_headers_p = PR_TRUE;
      msd->options->output_vcard_buttons_p = PR_TRUE;
      msd->options->fancy_links_p = PR_TRUE;
      break;

	case nsMimeOutput::nsMimeMessageSaveAs:         // Save As operations
	case nsMimeOutput::nsMimeMessageQuoting:        // all HTML quoted/printed output
  case nsMimeOutput::nsMimeMessagePrintOutput:
      msd->options->fancy_headers_p = PR_TRUE;
      msd->options->fancy_links_p = PR_TRUE;
      break;

	case nsMimeOutput::nsMimeMessageBodyQuoting:        // only HTML body quoted output
      MIME_HeaderType = MimeHeadersNone;
      break;

    case nsMimeOutput::nsMimeMessageRaw:              // the raw RFC822 data (view source) and attachments
		case nsMimeOutput::nsMimeMessageDraftOrTemplate:  // Loading drafts & templates
		case nsMimeOutput::nsMimeMessageEditorTemplate:   // Loading templates into editor
    case nsMimeOutput::nsMimeMessageFilterSniffer:    // generating an output that can be scan by a message filter
      break;

    case nsMimeOutput::nsMimeMessageDecrypt:
      msd->options->decrypt_p = PR_TRUE;
      msd->options->write_html_p = PR_FALSE;
      break;
  }

  ////////////////////////////////////////////////////////////
  // Now, get the libmime prefs...
  ////////////////////////////////////////////////////////////
  
  /* This pref is written down in with the
  opposite sense of what we like to use... */
  MIME_WrapLongLines = PR_TRUE;
  if (msd->options->prefs)
    msd->options->prefs->GetBoolPref("mail.wrap_long_lines", &MIME_WrapLongLines);

  MIME_VariableWidthPlaintext = PR_TRUE;
  if (msd->options->prefs)
    msd->options->prefs->GetBoolPref("mail.fixed_width_messages", &MIME_VariableWidthPlaintext);
  MIME_VariableWidthPlaintext = !MIME_VariableWidthPlaintext;

  msd->options->wrap_long_lines_p = MIME_WrapLongLines;
  msd->options->headers = MIME_HeaderType;
  
  // We need to have the URL to be able to support the various 
  // arguments
  status = mime_parse_url_options(msd->url_name, msd->options);
  if (status < 0)
  {
    PR_FREEIF(msd->options->part_to_load);
    PR_Free(msd->options);
    PR_Free(msd);
    return 0;
  }
 
  if (msd->options->headers == MimeHeadersMicro &&
     (msd->url_name == NULL || (strncmp(msd->url_name, "news:", 5) != 0 &&
              strncmp(msd->url_name, "snews:", 6) != 0)) )
    msd->options->headers = MimeHeadersMicroPlus;

  msd->options->url = msd->url_name;
  msd->options->output_init_fn        = mime_output_init_fn;
  
  msd->options->output_fn             = mime_output_fn;

  msd->options->whattodo 	      = whattodo;
  msd->options->charset_conversion_fn = mime_convert_charset;
  msd->options->rfc1522_conversion_p  = PR_TRUE;
  msd->options->file_type_fn          = mime_file_type;
  msd->options->stream_closure        = msd;
  msd->options->passwd_prompt_fn      = 0;
  
  msd->options->image_begin           = mime_image_begin;
  msd->options->image_end             = mime_image_end;
  msd->options->make_image_html       = mime_image_make_image_html;
  msd->options->image_write_buffer    = mime_image_write_buffer;
  
  msd->options->variable_width_plaintext_p = MIME_VariableWidthPlaintext;

  // 
  // Charset overrides takes place here
  //
  // We have a bool pref (mail.force_user_charset) to deal with attachments.
  // 1) If true - libmime does NO conversion and just passes it through to raptor
  // 2) If false, then we try to use the charset of the part and if not available, 
  //    the charset of the root message 
  //
  msd->options->force_user_charset = PR_FALSE;

  if (msd->options->prefs)
    msd->options->prefs->GetBoolPref("mail.force_user_charset", &(msd->options->force_user_charset));

  // If this is a part, then we should emit the HTML to render the data
  // (i.e. embedded images)
  if (msd->options->part_to_load && msd->options->format_out != nsMimeOutput::nsMimeMessageBodyDisplay)
    msd->options->write_html_p = PR_FALSE;

  if (msd->options->prefs)
    msd->options->prefs->GetBoolPref("mail.inline_attachments", &(msd->options->show_attachment_inline_p));

  obj = mime_new ((MimeObjectClass *)&mimeMessageClass, (MimeHeaders *) NULL, MESSAGE_RFC822);
  if (!obj)
  {
    delete msd->options;
    PR_Free(msd);
    return 0;
  }

  obj->options = msd->options;
  msd->obj = obj;
  
  /* Both of these better not be true at the same time. */
  PR_ASSERT(! (obj->options->decrypt_p && obj->options->write_html_p));
  
  stream = PR_NEW(nsMIMESession);
  if (!stream)
  {
    delete msd->options;
    PR_Free(msd);
    PR_Free(obj);
    return 0;
  }
  
  memset (stream, 0, sizeof (*stream));  
  stream->name           = "MIME Conversion Stream";
  stream->complete       = mime_display_stream_complete;
  stream->abort          = mime_display_stream_abort;
  stream->put_block      = mime_display_stream_write;
  stream->data_object    = msd;
  
  status = obj->clazz->initialize(obj);
  if (status >= 0)
    status = obj->clazz->parse_begin(obj);
  if (status < 0)
  {
    PR_Free(stream);
    delete msd->options;
    PR_Free(msd);
    PR_Free(obj);
    return 0;
  }
  
  return stream;
}

//
// Emitter Wrapper Routines!
//
nsIMimeEmitter *
GetMimeEmitter(MimeDisplayOptions *opt)
{
  mime_stream_data  *msd = (mime_stream_data *)opt->stream_closure;
  if (!msd) 
    return NULL;

  nsIMimeEmitter     *ptr = (nsIMimeEmitter *)(msd->output_emitter);
  return ptr;
}

mime_stream_data *
GetMSD(MimeDisplayOptions *opt)
{
  if (!opt)
    return nsnull;
  mime_stream_data  *msd = (mime_stream_data *)opt->stream_closure;
  return msd;
}

PRBool
NoEmitterProcessing(nsMimeOutputType    format_out)
{
  if ( (format_out == nsMimeOutput::nsMimeMessageDraftOrTemplate) ||
       (format_out == nsMimeOutput::nsMimeMessageEditorTemplate))
    return PR_TRUE;
  else
    return PR_FALSE;
}

extern "C" nsresult
mimeEmitterAddAttachmentField(MimeDisplayOptions *opt, const char *field, const char *value)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->AddAttachmentField(field, value);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterAddHeaderField(MimeDisplayOptions *opt, const char *field, const char *value)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->AddHeaderField(field, value);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterAddAllHeaders(MimeDisplayOptions *opt, const char *allheaders, const PRInt32 allheadersize)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->AddAllHeaders(allheaders, allheadersize);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterStartAttachment(MimeDisplayOptions *opt, const char *name, const char *contentType, const char *url,
                           PRBool aNotDownloaded)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->StartAttachment(name, contentType, url, aNotDownloaded);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterEndAttachment(MimeDisplayOptions *opt)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    if (emitter)
      return emitter->EndAttachment();
    else
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterEndAllAttachments(MimeDisplayOptions *opt)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    if (emitter)
      return emitter->EndAllAttachments();
    else
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterStartBody(MimeDisplayOptions *opt, PRBool bodyOnly, const char *msgID, const char *outCharset)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->StartBody(bodyOnly, msgID, outCharset);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterEndBody(MimeDisplayOptions *opt)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->EndBody();
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterEndHeader(MimeDisplayOptions *opt)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->EndHeader();
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterUpdateCharacterSet(MimeDisplayOptions *opt, const char *aCharset)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->UpdateCharacterSet(aCharset);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterStartHeader(MimeDisplayOptions *opt, PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                       const char *outCharset)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->StartHeader(rootMailHeader, headerOnly, msgID, outCharset);
  }

  return NS_ERROR_FAILURE;
}


extern "C" nsresult
mimeSetNewURL(nsMIMESession *stream, char *url)
{
  if ( (!stream) || (!url) || (!*url) )
    return NS_ERROR_FAILURE;

  mime_stream_data  *msd = (mime_stream_data *)stream->data_object;
  if (!msd)
    return NS_ERROR_FAILURE;

  char *tmpPtr = nsCRT::strdup(url);
  if (!tmpPtr)
    return NS_ERROR_FAILURE;

  PR_FREEIF(msd->url_name);
  msd->url_name = nsCRT::strdup(tmpPtr);
  return NS_OK;
}

#define     MIME_URL "chrome://messenger/locale/mime.properties"

extern "C" 
char *
MimeGetStringByID(PRInt32 stringID)
{
  char          *tempString = nsnull;
  const char    *resultString = "???";
  nsresult      res = NS_OK;

  if (!stringBundle)
  {
    char* propertyURL = NULL;

    propertyURL = MIME_URL;

    nsCOMPtr<nsIStringBundleService> sBundleService = 
             do_GetService(NS_STRINGBUNDLE_CONTRACTID, &res); 
    if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
    {
      res = sBundleService->CreateBundle(propertyURL, getter_AddRefs(stringBundle));
    }
  }

  if (stringBundle)
  {
    nsXPIDLString v;
    res = stringBundle->GetStringFromID(stringID, getter_Copies(v));

    if (NS_SUCCEEDED(res)) 
      tempString = ToNewUTF8String(v);
  }

  if (!tempString)
    tempString = nsCRT::strdup(resultString);

  return tempString;
}

void PR_CALLBACK
ResetChannelCharset(MimeObject *obj) 
{ 
  if (obj->options && obj->options->stream_closure && 
      obj->options->default_charset && obj->headers ) 
  { 
    mime_stream_data  *msd = (mime_stream_data *) (obj->options->stream_closure); 
    char *ct = MimeHeaders_get (obj->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE); 
    if ( (ct) && (msd) && (msd->channel) ) 
    { 
      char *ptr = strstr(ct, "charset="); 
      if (ptr)
      {
        // First, setup the channel!
        msd->channel->SetContentType(nsDependentCString(ct));

        // Second, if this is a Save As operation, then we need to convert
        // to override the output charset!
        mime_stream_data  *msd = GetMSD(obj->options);
        if ( (msd) && (msd->format_out == nsMimeOutput::nsMimeMessageSaveAs) )
        {
          // Extract the charset alone
          char  *cSet = nsnull;
          if (*(ptr+8) == '"')
            cSet = nsCRT::strdup(ptr+9);
          else
            cSet = nsCRT::strdup(ptr+8);
          if (cSet)
          {
            char *ptr2 = cSet;
            while ( (*cSet) && (*cSet != ' ') && (*cSet != ';') && 
                    (*cSet != nsCRT::CR) && (*cSet != nsCRT::LF) && (*cSet != '"') )
              ptr2++;
            
            if (*cSet) {
              PR_FREEIF(obj->options->default_charset);
              obj->options->default_charset = nsCRT::strdup(cSet);
              obj->options->override_charset = PR_TRUE;
            }

            PR_FREEIF(cSet);
          }
        }
      }
      PR_FREEIF(ct); 
    } 
  } 
}

  ////////////////////////////////////////////////////////////
  // Function to get up mail/news fontlang 
  ////////////////////////////////////////////////////////////


nsresult GetMailNewsFont(MimeObject *obj, PRBool styleFixed,  PRInt32 *fontPixelSize, 
		                     PRInt32 *fontSizePercentage, nsCString& fontLang)
{
  nsresult rv = NS_OK;

  nsIPref *prefs = GetPrefServiceManager(obj->options);
  if (prefs) {
    MimeInlineText  *text = (MimeInlineText *) obj;
    nsCAutoString charset;

    // get a charset
    if (!text->initializeCharset)
      ((MimeInlineTextClass*)&mimeInlineTextClass)->initialize_charset(obj);

    if (!text->charset || !(*text->charset))
      charset.Assign("us-ascii");
    else
      charset.Assign(text->charset);

    nsCOMPtr<nsICharsetConverterManager> charSetConverterManager2;
    nsCOMPtr<nsIAtom> langGroupAtom;
    nsCAutoString prefStr;

    ToLowerCase(charset);

    charSetConverterManager2 = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
    if ( NS_FAILED(rv))
      return rv;

    // get a language, e.g. x-western, ja
    rv = charSetConverterManager2->GetCharsetLangGroup(charset.get(), getter_AddRefs(langGroupAtom));
    if (NS_FAILED(rv))
      return rv;
    rv = langGroupAtom->ToUTF8String(fontLang);
    if (NS_FAILED(rv))
      return rv;

    // get a font size from pref
    prefStr.Assign(!styleFixed ? "font.size.variable." : "font.size.fixed.");
    prefStr.Append(fontLang);
    rv = prefs->GetIntPref(prefStr.get(), fontPixelSize);
    if (NS_FAILED(rv))
      return rv;

    // get original font size
    PRInt32 originalSize;
    rv = prefs->GetDefaultIntPref(prefStr.get(), &originalSize);
    if (NS_FAILED(rv))
      return rv;

    // calculate percentage
    *fontSizePercentage = originalSize ? 
                          (PRInt32)((float)*fontPixelSize / (float)originalSize * 100) : 0;

  }

  return NS_OK;
}

/* This function syncronously converts an HTML document (as string)
   to plaintext (as string) using the Gecko converter.

   flags: see nsIDocumentEncoder.h
*/
// TODO: |printf|s?
/* <copy from="mozilla/parser/htmlparser/test/outsinks/Convert.cpp"
         author="akk"
         adapted-by="Ben Bucksch"
         comment=" 'This code would not have been possible without akk.' ;-P.
                   No, really. "
   > */
nsresult
HTML2Plaintext(const nsString& inString, nsString& outString,
               PRUint32 flags, PRUint32 wrapCol)
{
  nsresult rv = NS_OK;

#if DEBUG_BenB
  printf("Converting HTML to plaintext\n");
  char* charstar = ToNewUTF8String(inString);
  printf("HTML source is:\n--------------------\n%s--------------------\n",
         charstar);
  delete[] charstar;
#endif

  // Create a parser
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID);
  NS_ENSURE_TRUE(parser, NS_ERROR_FAILURE);

  // Create the appropriate output sink
  nsCOMPtr<nsIContentSink> sink =
                               do_CreateInstance(NS_PLAINTEXTSINK_CONTRACTID);
  NS_ENSURE_TRUE(sink, NS_ERROR_FAILURE);

  nsCOMPtr<nsIHTMLToTextSink> textSink(do_QueryInterface(sink));
  NS_ENSURE_TRUE(textSink, NS_ERROR_FAILURE);

  textSink->Initialize(&outString, flags, wrapCol);

  parser->SetContentSink(sink);
  nsCOMPtr<nsIDTD> dtd = do_CreateInstance(kNavDTDCID);
  NS_ENSURE_TRUE(dtd, NS_ERROR_FAILURE);

  parser->RegisterDTD(dtd);

  rv = parser->Parse(inString, 0, NS_LITERAL_CSTRING("text/html"),
                     PR_FALSE, PR_TRUE);

  // Aah! How can NS_ERROR and NS_ABORT_IF_FALSE be no-ops in release builds???
  if (NS_FAILED(rv))
  {
    NS_ERROR("Parse() failed!");
    return rv;
  }

#if DEBUG_BenB
  charstar = ToNewUTF8String(outString);
  printf("Plaintext is:\n--------------------\n%s--------------------\n",
         charstar);
  delete[] charstar;
#endif

  return rv;
}
// </copy>



/* This function syncronously sanitizes an HTML document (string->string)
   using the Gecko ContentSink mozISanitizingHTMLSerializer.

   flags: currently unused
   allowedTags: see mozSanitizingHTMLSerializer::ParsePrefs()
*/
// copied from HTML2Plaintext above
nsresult
HTMLSanitize(const nsString& inString, nsString& outString,
             PRUint32 flags, const nsAString& allowedTags)
{
  nsresult rv = NS_OK;

#if DEBUG_BenB
  printf("Sanitizing HTML\n");
  char* charstar = ToNewUTF8String(inString);
  printf("Original HTML is:\n--------------------\n%s--------------------\n",
         charstar);
  delete[] charstar;
#endif

  // Create a parser
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID);
  NS_ENSURE_TRUE(parser, NS_ERROR_FAILURE);

  // Create the appropriate output sink
  nsCOMPtr<nsIContentSink> sink =
                    do_CreateInstance(MOZ_SANITIZINGHTMLSERIALIZER_CONTRACTID);
  NS_ENSURE_TRUE(sink, NS_ERROR_FAILURE);

  nsCOMPtr<mozISanitizingHTMLSerializer> sanSink(do_QueryInterface(sink));
  NS_ENSURE_TRUE(sanSink, NS_ERROR_FAILURE);

  sanSink->Initialize(&outString, flags, allowedTags);

  parser->SetContentSink(sink);
  nsCOMPtr<nsIDTD> dtd = do_CreateInstance(kNavDTDCID);
  NS_ENSURE_TRUE(dtd, NS_ERROR_FAILURE);

  parser->RegisterDTD(dtd);

  rv = parser->Parse(inString, 0, NS_LITERAL_CSTRING("text/html"),
                     PR_FALSE, PR_TRUE);
  if (NS_FAILED(rv))
  {
    NS_ERROR("Parse() failed!");
    return rv;
  }

#if DEBUG_BenB
  charstar = ToNewUTF8String(outString);
  printf("Sanitized HTML is:\n--------------------\n%s--------------------\n",
         charstar);
  delete[] charstar;
#endif

  return rv;
}
