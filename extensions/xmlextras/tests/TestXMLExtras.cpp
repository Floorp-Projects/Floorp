/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *                 Heikki Toivonen <heikki@netscape.com>
 *
 */

#include <nsCOMPtr.h>
#include <nsString.h>
#include <nsIURI.h>
#include <nsIChannel.h>
#include <nsIInputStream.h>
#include <nsIDOMDocument.h>
#include <nsIDOMParser.h>
#include <nsIXMLHttpRequest.h>
#include <nsNetUtil.h>
#include <nsIDOMElement.h>
#include "nsIDocument.h"

void usage( ) {
  printf( "\n" );
  printf( "Usage:\n" );
  printf( "  TestXMLExtras {parsestr | parse | syncread} xmlfile\n\n" );
  printf( "    parsestr = Invokes DOMParser against a string supplied as second argument\n" );
  printf( "    parse    = Invokes DOMParser against a local file\n" );
  printf( "      xmlfile  = A local XML URI (ie. file:///d:/file.xml)\n\n" );
  printf( "    syncread = Invokes XMLHttpRequest for a synchronous read\n" );
  printf( "      xmlfile  = An XML URI (ie. http://www.w3.org/TR/P3P/base\n\n" );

  return;
}


int main (int argc, char* argv[]) 
{
  nsresult                     rv;
  nsCOMPtr<nsIURI>             pURI;
  nsCOMPtr<nsIChannel>         pChannel;
  nsCOMPtr<nsIInputStream>     pInputStream;
  PRUint32                     uiContentLength;
  nsCOMPtr<nsIDOMParser>       pDOMParser;
  nsCOMPtr<nsIDOMDocument>     pDOMDocument;
  nsCOMPtr<nsIXMLHttpRequest>  pXMLHttpRequest;

  if (argc > 2) {
    if (nsCRT::strcasecmp( argv[1], "parsestr" ) == 0) {
      pDOMParser = do_CreateInstance( NS_DOMPARSER_CONTRACTID,
                                     &rv );

      if (NS_SUCCEEDED( rv )) {
        nsString str; str.AssignWithConversion(argv[2]);
        rv = pDOMParser->ParseFromString(str.GetUnicode(),"text/xml",
                                          getter_AddRefs( pDOMDocument ) );

        if (NS_SUCCEEDED( rv )) {
          printf( "DOM parse string of\n\n%s\n\nsuccessful\n", argv[2] );
        }
        else {
          printf( "DOM parse of \n%s\n NOT successful\n", argv[2] );
        }
      }
      else {
        printf( "do_CreateInstance of DOMParser failed for %s - %08X\n", argv[2], rv );
      }
    } else if (nsCRT::strcasecmp( argv[1], "parse" ) == 0) {
      // DOM Parser
      rv = NS_NewURI( getter_AddRefs( pURI ),
                      argv[2] );

      if (NS_SUCCEEDED( rv )) {
        rv = NS_OpenURI( getter_AddRefs( pChannel ),
                         pURI,
                         nsnull,
                         nsnull );

        if (NS_SUCCEEDED( rv )) {
          rv = pChannel->OpenInputStream( getter_AddRefs( pInputStream ) );

          if (NS_SUCCEEDED( rv )) {
            rv = pInputStream->Available(&uiContentLength );

            if (NS_SUCCEEDED( rv )) {
              pDOMParser = do_CreateInstance( NS_DOMPARSER_CONTRACTID,
                                             &rv );

              if (NS_SUCCEEDED( rv )) {
                pDOMParser->SetBaseURI(pURI);

                rv = pDOMParser->ParseFromStream( pInputStream,
                                                  "UTF-8",
                                                  uiContentLength,
                                                  "text/xml",
                                                  getter_AddRefs( pDOMDocument ) );
                if (NS_SUCCEEDED( rv )) {
                  printf( "DOM parse of %s successful\n", argv[2] );
                }
                else {
                  printf( "DOM parse of %s NOT successful\n", argv[2] );
                }
              }
              else {
                printf( "do_CreateInstance of DOMParser failed for %s - %08X\n", argv[2], rv );
              }
            }
            else {
              printf( "pInputSteam->Available failed for %s - %08X\n", argv[2], rv );
            }
          }
          else {
            printf( "pChannel->OpenInputStream failed for %s - %08X\n", argv[2], rv );
          }
        }
        else {
          printf( "NS_OpenURI failed for %s - %08X\n", argv[2], rv );
        }
      }
      else {
        printf( "NS_NewURI failed for %s - %08X\n", argv[2], rv );
      }
    }
    else if (nsCRT::strcasecmp( argv[1], "syncread" ) == 0) {
      // Synchronous Read
      pXMLHttpRequest = do_CreateInstance( NS_XMLHTTPREQUEST_CONTRACTID,
                                          &rv );

      if (NS_SUCCEEDED( rv )) {
        rv = pXMLHttpRequest->OpenRequest( "GET",
                                           argv[2],
                                           PR_FALSE,
                                           nsnull, nsnull );

        if (NS_SUCCEEDED( rv )) {
          rv = pXMLHttpRequest->Send( nsnull );

          if (NS_SUCCEEDED( rv )) {
            rv = pXMLHttpRequest->GetResponseXML( getter_AddRefs( pDOMDocument ) );

            if (NS_SUCCEEDED( rv )) {

              if (pDOMDocument) {
                printf( "Synchronous read of %s successful, DOMDocument created\n", argv[2] );
              }
              else {
                printf( "Synchronous read of %s NOT successful, DOMDocument NOT created\n", argv[2] );
              }
            }
            else {
              printf( "pXMLHttpRequest->GetResponseXML failed for %s - %08X\n", argv[2], rv );
            }
          }
          else {
            printf( "pXMLHttpRequest->Send failed for %s - %08X\n", argv[2], rv );
          }
        }
        else {
          printf( "pXMLHttpRequest->OpenRequest failed for %s - %08X\n", argv[2], rv );
        }
      }
      else {
        printf( "do_CreateInstance of XMLHttpRequest failed for %s - %08X\n", argv[2], rv );
      }
    }
    else {
      usage( );
    }
  }
  else {
    usage( );
  }

  if (pDOMDocument) {
    nsCOMPtr<nsIDOMElement> element;
    pDOMDocument->GetDocumentElement(getter_AddRefs(element));
    nsAutoString tagName;
    if (element) element->GetTagName(tagName);
    char *s = tagName.ToNewCString();
    printf("Document element=\"%s\"\n",s);
    nsCRT::free(s);
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(pDOMDocument);
    if (doc) {
      nsCOMPtr<nsIURI> uri = dont_AddRef(doc->GetDocumentURL());
      nsXPIDLCString spec;
      uri->GetSpec(getter_Copies(spec));
      printf("Document URI=\"%s\"\n",spec.get());
    }
  }

  return 0;
}
