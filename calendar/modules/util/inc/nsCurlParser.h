/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2
-*- 
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

/**
 * This class manages a string in one of the following forms:
 *
 *    protocol://user:password@host.domain[:port]/CSID[:extra]
 *    mailto:user@host.domain
 *
 * Examples:
 *    capi://calendar-1.mcom.com/sman
 *    capi://jason:badguy@calendar-1.mcom.com/sman
 *    capi://localhost/c|/temp/junk.ics
 *    mailto:sman@netscape.com
 *
 * The component parts of a calendar URL are:
 *    protocol:   currently supported are CAPI, IMIP (mailto), and IRIP
 *    user:       the user making the access
 *    password:   the password for the user
 *    host:       the host.domain where the calendar server resides
 *    port:       optional, tells what port to use
 *    CSID:       a unique string identifying a calendar store. It
 *                includes everything past the host.domain slash up to
 *                the end of the line or to the first colon (:) 
 *                delimiting the extra stuff
 *    extra:      extra information that may be needed by a particular
 *                server or service.
 *
 * This class provides quick access to these component parts.
 * Expected protocols are:  CAPI, IRIP, MAILTO
 *
 * This code probably exists somewhere else.
 *
 * sman
 *
 * Modification:  Code supports converting from "file:" and "resource:"
 * to platform specific file handle.
 *
 */
#ifndef _NS_CURL_PARSER_H
#define _NS_CURL_PARSER_H

#include "nscalutilexp.h"

class NS_CAL_UTIL nsCurlParser
{
public:
  enum ePROTOCOL {eUNKNOWN, eCAPI, eIMIP, eIRIP, eFILE, eRESOURCE, 
                  eHTTP, eFTP, eENDPROTO};

private:
  JulianString  m_sCurl;
  ePROTOCOL     m_eProto;
  JulianString  m_sUser;
  JulianString  m_sPassword;
  JulianString  m_sHost;
  PRInt32       m_iPort;
  JulianString  m_sCSID;
  JulianString  m_sExtra;
  PRBool        m_bParseNeeded;
  PRBool        m_bAssemblyNeeded;
  
  void          Parse();
  void          Assemble();
  void          Init();
  void          Init(const nsCurlParser& curl);

public:
                nsCurlParser();
                nsCurlParser(const char* psCurl);
                nsCurlParser(const JulianString& sCurl);
                nsCurlParser(const nsCurlParser& curl);
  virtual       ~nsCurlParser();

  ePROTOCOL     GetProtocol();
  JulianString  GetProtocolString();
  JulianString  GetHost();
  JulianString  GetUser();
  JulianString  GetPassword();
  PRInt32       GetPort();
  JulianString  GetCSID();
  JulianString  GetPath();
  JulianString  GetExtra();

  /**
   * @return a JulianString containing the fully qualified Calendar URL.
   */
  JulianString  GetCurl();

  /**
   *  Set the values of missing fields of *this using that
   *  as a template.
   *  @param that  the curl parser to use as a template
   *  @return   a reference to *this
   */
  nsCurlParser& operator|=(nsCurlParser& that);

  /**
   *  Copy the supplied null terminated string into *this
   *  as the new value of the curl
   *  @param s  the character array to be copied.
   *  @return   a reference to *this
   */
  nsCurlParser& operator=(const char* s);

  /**
   *  Copy the supplied JulianString into *this as the new
   *  value of the curl.
   *  @param s  the string to be copied.
   *  @return   a reference to *this
   */
  nsCurlParser& operator=(JulianString& s);

  /**
   *  Copy the values from that into *this
   *  @param that  another nsCurlParser to use as a template
   *  @return   a reference to *this
   */
  nsCurlParser& operator=(const nsCurlParser& that);

  /**
   *  Set the curl to the supplied string.
   *  @param sCurl  new calendar url for this object
   *  @return   0 on success
   */
  nsresult      SetCurl(const JulianString& sCurl);

  /**
   *  Set the curl to the supplied string.
   *  @param psCurl  new calendar url for this object
   *  @return   0 on success
   */
  nsresult      SetCurl(const char* psCurl);

  /**
   *  Set the protocol for this curl to one of the enumerated types
   *  @param e  the protocol
   *  @return   0 on success
   */
  nsresult      SetProtocol(ePROTOCOL e);
  nsresult      SetHost(const JulianString& sHost);
  nsresult      SetHost(const char* sHost);
  nsresult      SetPort(PRInt32 iPort);
  nsresult      SetUser(const JulianString& sUser);
  nsresult      SetUser(const char* sUser);
  nsresult      SetPassword(const JulianString& sPassword);
  nsresult      SetPassword(const char* sPassword);
  nsresult      SetCSID(const JulianString& sCSID);
  nsresult      SetCSID(const char* sCSID);
  nsresult      SetPath(const char* sPath);
  nsresult      SetPath(const JulianString& sPath);
  nsresult      SetExtra(const JulianString& sExtra);
  nsresult      SetExtra(const char* sExtra);

  /**
   * Encode any illegal characters in username, password, CSID, 
   * and extra fields. No status is kept on whether or not any
   * of the fields are currently encoded or not. The encoding
   * is done blindly.
   * @result NS_OK on success
   */
  nsresult URLEncode();

  /**
   * Decode any illegal characters in username, password, CSID, 
   * and extra fields. No status is kept on whether or not any
   * of the fields is currently decoded or not. The decoding is
   * done blindly.
   * @result NS_OK on success
   */
  nsresult URLDecode();

  /**
   * Filter the characters in the CSID portion of the curl so that it
   * is in acceptable format for OS specific file operations.
   * @result 0 on success
   */
  nsresult ResolveFileURL();

  /**
   * @return a JulianString the path portion of the CSID
   */
  JulianString  CSIDPath();

  static void   ConvertToURLFileChars(JulianString& s);
  static void   ConvertToFileChars(JulianString& s);

  NS_METHOD_(char *)  ToLocalFile();
  NS_METHOD_(PRBool)  IsLocalFile();
  NS_IMETHOD_(char *) LocalFileToURL();

private:
  NS_METHOD_(char *) ResourceToFile();
};

#endif  /* _NS_CURL_PARSER_H */
