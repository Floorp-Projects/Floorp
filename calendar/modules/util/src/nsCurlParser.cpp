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
 *    protocol://[user[:password]@]host.domain[:port]/CSID[:extra]
 *    mailto:user@host.domain
 *
 * Examples:
 *    capi://calendar-1.mcom.com/sman
 *    capi://jason:badguy@calendar-1.mcom.com/sman
 *    capi://guest@calendar-1.mcom.com/sman
 *    file://localhost/c|/temp/junk.ics
 *    file:///c|/temp/junk.ics
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
 *    remainder:  Anything past an embedded space. When the URL is parsed
 *                it is expected not to include embedded spaces. When a
 *                space is found, it could be that the string is
 *                actually a list of URLs. The characters to the right
 *                of the space are stored in remainder.
 *
 * This class provides quick access to these component parts.
 * Expected protocols are:  CAPI, IRIP, MAILTO, FILE, HTTP, FTP, RESOURCE
 *
 * This code probably exists somewhere else.
 *
 * sman
 *
 * Modification:  Code supports converting from "file:" and "resource:"
 * to platform specific file handle.
 *
 */

#ifdef XP_PC
#include "windows.h"
#endif

#include "nscalcore.h"
#include "nsString.h"
#include "xp_mcom.h"
#include "jdefines.h"
#include "julnstr.h"
#include "nspr.h"
#include "plstr.h"
#include "nsCurlParser.h"


static char *asProtocols[] =
{
  "",
  "capi",
  "irip",
  "imip",
  "file",
  "resource",
  "http",
  "ftp",
  0
};

nsCurlParser::nsCurlParser()
{
  m_sCurl = "";
  Init();
}

nsCurlParser::nsCurlParser(const JulianString& sCurl)
{
  Init();
  SetCurl(sCurl);
}

nsCurlParser::nsCurlParser(const char* psCurl)
{
  Init();
  SetCurl(psCurl);
}

nsCurlParser::nsCurlParser(const nsCurlParser& that)
{
  Init(that);
}

nsCurlParser::~nsCurlParser()
{
}

/**
 *  Initialize all internal variables...
 */
void nsCurlParser::Init()
{
  m_eProto = eUNKNOWN;
  m_sHost = "";
  m_sUser = "";
  m_sPassword = "";
  m_iPort = -1;
  m_sCSID = "";
  m_sExtra = "";
  m_sRemainder = "";
  m_bParseNeeded = PR_FALSE;
  m_bAssemblyNeeded = PR_FALSE;
}

/**
 *  Initialize all internal variables using *that as a template
 */
void nsCurlParser::Init(const nsCurlParser& that)
{
  m_eProto = that.m_eProto;
  m_sHost = that.m_sHost;
  m_iPort = that.m_iPort;
  m_sCSID = that.m_sCSID;
  m_sExtra = that.m_sExtra;
  m_sRemainder = that.m_sRemainder;
  m_bParseNeeded = that.m_bParseNeeded;
  m_bAssemblyNeeded = that.m_bAssemblyNeeded;
}
/**
 * Assemble a string that represents the CURL...
 */
void nsCurlParser::Assemble()
{
  char sBuf[100];

  if (m_bAssemblyNeeded)
  {
    m_sCurl  = GetProtocolString();
    m_sCurl += "://";
    if (m_sUser != "" || m_sPassword != "")
    {
      if (m_sUser != "")
        m_sCurl += m_sUser;
      if (m_sPassword != "")
      {
        m_sCurl += ":";
        m_sCurl += m_sPassword;
      }
      m_sCurl += "@";
    }
    m_sCurl += m_sHost;
    if (m_iPort > 0)
    {
      sprintf(sBuf,"%d",m_iPort);
      m_sCurl += sBuf;
    }
    m_sCurl += "/";
    m_sCurl += m_sCSID;
    if (m_sExtra != "")
    {
      m_sCurl += "#";
      m_sCurl += m_sExtra;
    }
    m_bAssemblyNeeded = PR_FALSE;
  }
}

/**
 * Parse the curl into its component parts
 */
void nsCurlParser::Parse()
{
  int i,iHead, iTail;
  JulianString s;

  Init();

  /*
   * quick check: look for an embedded space. If one is found 
   * all characters to the right are stored in m_sRemainder.
   */
  if (-1 != (i = m_sCurl.Find(' ',0)))
  {
    m_sRemainder = m_sCurl.Right( m_sCurl.GetStrlen() - i );
    m_sCurl.GetBuffer()[i] = 0;
    m_sCurl.DoneWithBuffer();
  }

  m_eProto = eUNKNOWN;

  /*
   * Was a protocol specified?
   */ 
  iHead = m_sCurl.Find(':',0);
  if ( 0 > iHead)
  {
    iHead = iTail = 0;
  }
  else
  {
    s = m_sCurl(0,iHead);  /* copy everything up to the colon... */

    /*
     * try to match the protocol...
     */
    for (i = 0; asProtocols[i]; i++)
    {
      if (0 == s.CompareTo(asProtocols[i],PR_TRUE))
      {
        m_eProto = (ePROTOCOL)i;
        break;
      }
    }
  }

  /*
   * The host name terminates at the next slash or end of string...
   */
  if (m_sCurl[iHead] == ':')
    ++iHead;
  if ( m_sCurl[iHead] == '/' && m_sCurl[iHead+1] == '/')
  {
    /*
     * If the next character is a slash, the host name is null or "localhost"
     */
    if ( '/' == m_sCurl[iHead + 2])
    {
      m_sHost = "localhost";
    }
    else
    {
      iHead += 2;
      iTail = m_sCurl.Find('/',iHead);
      if (0 > iTail)
      {
        /*
         * no trailing '/'  Whatever is left goes into m_sHost
         */
        m_sHost = m_sCurl.indexSubstr(iHead,m_sCurl.GetStrlen());
        iTail = iHead;
      }
      else
      {
        m_sHost = m_sCurl.indexSubstr(iHead,iTail-1);
      }
    }
  }

  /*
   * Check for a username, password, and port number.
   * The general form for the host part is:
   *    <username>:<password>@<host>.<domain>:<port>
   */ 
  if (m_sHost != "")
  {
    int iTmpHead, iTmpTail;
    /*
     * Look for a username/password. If it is present there
     * will be an '@' character.
     */
    iTmpHead = m_sHost.Find('@',0);
    if (iTmpHead > 0)
    {
      m_sUser = m_sHost.Left(iTmpHead);
      /*
       * There may be a password embedded in the user name...
       */
      iTmpTail = m_sUser.Find(':');
      if (iTmpTail >= 0)
      {
        m_sPassword = m_sUser.Right(m_sUser.GetStrlen()-iTmpTail-1);
        m_sUser.GetBuffer()[iTmpTail] = 0;
        m_sUser.DoneWithBuffer();
      }

      JulianString sTemp = m_sHost.indexSubstr(iTmpHead+1,m_sHost.GetStrlen());
      m_sHost = sTemp;
    }

    /*
     * look for a port number. If it is present there will
     * be a ':' present.
     */
    iTmpHead = m_sHost.Find(':',0);
    if (iTmpHead > 0)
    {
      int iPort;
      if (1 == sscanf( m_sHost.GetBuffer(),"%d",&iPort))
      {
        m_iPort = iPort;
        m_sHost.GetBuffer()[iTmpHead] = 0;
        m_sHost.DoneWithBuffer();
      }
    }
  }

  /*
   * Everything after the "/" is the CSID. It may have "extra" info
   */
  if (m_sCurl[iTail] == '/')
    ++iTail;
  if (m_sCurl.GetStrlen() > (size_t) iTail)
  {
    m_sCSID = m_sCurl(iTail,m_sCurl.GetStrlen());
    iHead = m_sCSID.Find('#',0);
    if (iHead >= 0)
    {
      ++iHead;
      if (m_sCSID.GetStrlen() > (size_t)iHead)
      {
        m_sExtra = m_sCSID(iHead, m_sCSID.GetStrlen());
        m_sCSID.GetBuffer()[iHead-1] = 0;
        m_sCSID.DoneWithBuffer();
      }
    }
  }

  m_bAssemblyNeeded = PR_TRUE;
}

/**
 * Encode any illegal characters in username, password, CSID, 
 * and extra fields. No status is kept on whether or not any
 * of the fields are currently encoded or not. The encoding
 * is done blindly.
 * @result NS_OK on success
 */
nsresult nsCurlParser::URLEncode()
{
  m_sUser.URLEncode();
  m_sPassword.URLEncode();
  m_sCSID.URLEncode();
  m_sExtra.URLEncode();
  m_bAssemblyNeeded = PR_TRUE;
  return NS_OK;
}


/**
 * Decode any illegal characters in username, password, CSID, 
 * and extra fields. No status is kept on whether or not any
 * of the fields is currently decoded or not. The decoding is
 * done blindly.
 * @result NS_OK on success
 */
nsresult nsCurlParser::URLDecode()
{
  m_sUser.URLDecode();
  m_sPassword.URLDecode();
  m_sCSID.URLDecode();
  m_sExtra.URLDecode();
  m_bAssemblyNeeded = PR_TRUE;
  return NS_OK;
}

/**
 * @return a curl string
 */
JulianString nsCurlParser::GetCurl()
{
  if (m_bAssemblyNeeded)
    Assemble();
  return m_sCurl;
}

/**
 * Set a new value for this Curl. Parse it into its component parts
 * @return 0 = success
 */
nsresult nsCurlParser::SetCurl(const JulianString& sCurl)
{
  m_sCurl = sCurl;
  Parse();
  return 0;
}

nsresult nsCurlParser::SetCurl(const char* psCurl)
{
  m_sCurl = psCurl;
  Parse();
  return 0;
}

/**
 *  Copy the values from that into *this
 *  @param that  template from which values are copied
 *  @return   a reference to *this
 */
nsCurlParser& nsCurlParser::operator=(const nsCurlParser& that)
{
  Init(that);
  return *this;
}

/**
 *  Copy the supplied null terminated string into *this
 *  as the new value of the curl
 *  @param s  the character array to be copied.
 *  @return   a reference to *this
 */
nsCurlParser& nsCurlParser::operator=(const char* s)
{
  SetCurl(s);
  return *this;
}

/**
 *  Copy the supplied JulianString into *this as the new
 *  value of the curl.
 *  @param s  the string to be copied.
 *  @return   a reference to *this
 */
nsCurlParser& nsCurlParser::operator=(JulianString& s)
{
  SetCurl(s);
  return *this;
}

/**
 *  @return   the "path" portion of the CSID.
 *            
 */
JulianString nsCurlParser::CSIDPath()
{
  nsCurlParser tmp(*this);                    /* need our own copy to normalize */
  ConvertToURLFileChars(tmp.m_sCSID);         /* normalize the chars */
  PRInt32 iTail = tmp.GetCSID().RFind('/');   /* is the session qualified? */
  if  (-1 != iTail)
  {
    return tmp.GetCSID().indexSubstr(0,iTail);
  }
  else
    return "";
}

/**
 *  Fill in any missing fields in *this with fields from that. This routine
 *  essentially fully qualifies *this to the extent of that.
 *  @param that  Curl to use for filling in fields
 *  @return   a reference to *this
 */
nsCurlParser& nsCurlParser::operator|=(nsCurlParser& that)
{
  if (eUNKNOWN == m_eProto)
  {
    m_eProto = that.m_eProto;
    m_bAssemblyNeeded = PR_TRUE;
  }

  if (m_sHost == "")
  {
    m_sHost = that.m_sHost;
    m_bAssemblyNeeded = PR_TRUE;
  }

  if (-1 == m_iPort)
  {
    m_iPort = that.m_iPort;
    m_bAssemblyNeeded = PR_TRUE;
  }

  if (m_sCSID == "")
  {
    m_sCSID = that.m_sCSID;
    m_bAssemblyNeeded = PR_TRUE;
  }


  /*
   * Here we do a bit of special handling for FILE types. 
   * If *this is not a 
   * qualified path and *that is a qualified path, we'll merge the
   * path for that into this.
   */
  if (eFILE == m_eProto)
  {
    if ( -1 == m_sCSID.RFind('/'))                /* is the file name qualified? */
    {                                             /* no: maybe the session is qualified */
      nsCurlParser tmp(that);                     /* need our own copy to normalize */
      JulianString sTemp = tmp.CSIDPath();
      m_sCSID.Prepend(sTemp);                     /* use the sessions qualifier */
    }
  }
 
  if (m_sExtra == "")
  {
    m_sExtra = that.m_sExtra;
    m_bAssemblyNeeded = PR_TRUE;
  }

  return *this;
}

/**
 * Set the protocol for this curl
 * @return 0 = success
 *         1 = bad protocol
 */
nsresult nsCurlParser::SetProtocol(nsCurlParser::ePROTOCOL e)
{
  if ( eUNKNOWN <= e && e < eENDPROTO )
  {
    if (e != m_eProto)
    {
      m_eProto = e;
      m_bAssemblyNeeded = PR_TRUE;
    }
    return 0;
  }
  return 1;
}

/**
 * Get the protocol enumeration for this curl
 */
nsCurlParser::ePROTOCOL nsCurlParser::GetProtocol()
{
  return m_eProto;
}

/**
 * Get the protocol enumeration for this curl
 * @return the string associated with the current protocol
 */
JulianString nsCurlParser::GetProtocolString()
{
  return asProtocols[(int)m_eProto];
}

/**
 * Set the user for this curl
 * @param sUser the user name
 * @return 0 on success
 */
nsresult nsCurlParser::SetUser(const JulianString& sUser)
{
  m_sUser = sUser;
  m_bAssemblyNeeded = PR_TRUE;
  return 0;
}

/**
 * Set the user for this curl
 * @param sUser the user name
 * @return 0 on success
 */
nsresult nsCurlParser::SetUser(const char* sUser)
{
  m_sUser = sUser;
  m_bAssemblyNeeded = PR_TRUE;
  return 0;
}

/**
 * Get the user name
 * @result an JulianString containing the current user name.
 */
JulianString nsCurlParser::GetUser()
{
  if (m_bAssemblyNeeded)
    Assemble();
  return m_sUser;
}

/**
 * Set the password for this curl
 * @param sPassword the password
 * @return 0 on success
 */
nsresult nsCurlParser::SetPassword(const JulianString& sPassword)
{
  m_sPassword = sPassword;
  m_bAssemblyNeeded = PR_TRUE;
  return 0;
}

nsresult nsCurlParser::SetPassword(const char* sPassword)
{
  m_sPassword = sPassword;
  m_bAssemblyNeeded = PR_TRUE;
  return 0;
}

/**
 * Get the user name
 * @result an JulianString containing the current user name.
 */
JulianString nsCurlParser::GetPassword()
{
  if (m_bAssemblyNeeded)
    Assemble();
  return m_sPassword;
}

/**
 * Set the host for this curl
 * @param s the host name
 * @return 0 on success
 */
nsresult nsCurlParser::SetHost(const JulianString& s)
{
  m_sHost = s;
  m_bAssemblyNeeded = PR_TRUE;
  return 0;
}

nsresult nsCurlParser::SetHost(const char * s)
{
  m_sHost = s;
  m_bAssemblyNeeded = PR_TRUE;
  return 0;
}

/**
 * Get the host name
 * @result an JulianString containing the current host name.
 */
JulianString nsCurlParser::GetHost()
{
  if (m_bAssemblyNeeded)
    Assemble();
  return m_sHost;
}

/**
 * Set the tcp port
 * @result 0 on success
 */
nsresult nsCurlParser::SetPort(PRInt32 iPort)
{
  if (m_iPort != iPort)
  {
    m_iPort = iPort;
    m_bAssemblyNeeded = PR_TRUE;
  }
  return 0;
}

/**
 * Set the CSID (the calendar store id) for this curl
 * @result 0 on success
 */
nsresult nsCurlParser::SetCSID(const JulianString& s)
{
  m_sCSID = s;
  m_bAssemblyNeeded = PR_TRUE;
  return 0;
}

nsresult nsCurlParser::SetCSID(const char* s)
{
  if (m_sCSID.CompareTo(s,PR_TRUE))
  {
    m_sCSID = s;
    m_bAssemblyNeeded = PR_TRUE;
  }
  return 0;
}

/**
 * Get the CSID
 * @return an JulianString containing the current CSID.
 */
JulianString nsCurlParser::GetCSID()
{
  return m_sCSID;
}

/**
 * The CSID can be a fully qualified file path or a hierarchical
 * name. When this is the case, it is useful to be able to retrieve
 * the path portion of the name. This routine returns the path
 * portion. Examples
 *    m_sCSID              GetPath
 *    -------------------  --------------------
 *    c|/temp/cal/j1.ics   c|/temp/cal/
 *    c:/temp/cal/j2.ics   c:/temp/cal/
 *    /tmp/j3              /tmp
 *    /j4                  /
 *    j5                   <empty string>
 *
 * @return the path string as defined above
 */
JulianString nsCurlParser::GetPath()
{
  PRInt32 iTail = m_sCSID.RFind('/');

  if ( -1 == iTail )
  {
    return "";
  }
  else
  {
    return m_sCSID.indexSubstr(0,iTail);
  }
}

/**
 * Replace the path with the supplied path. The supplied path
 * may or may not end with a '/' character. Examples:
 *
 * <pre>
 * m_sCSID            p          resulting m_sCSID
 * ------------------ ---------- --------------------
 * d:/x/y/z/a/sman    c:/cal     c:/cal/sman
 * sman               c:/cal     c:/cal/sman
 * sman               c:/cal/    c:/cal/sman
 * </pre>
 *
 * @return 0  success
 */
nsresult nsCurlParser::SetPath(const char* p)
{
  JulianString s;

  /*
   *  Is there a path currently?
   */
  PRInt32 iTail = m_sCSID.RFind('/');

  s = p;
  if ( -1 == iTail )
  {
    /*
     *  No path at this time. Set the path to the supplied value.
     */
    if ( '/' != s[s.GetStrlen()-1] )
      s += "/";
    m_sCSID.Prepend(s);
  }
  else
  {
    s += m_sCSID.Right(s.GetStrlen()-iTail-1);
  }
  return 0;
}

/**
 * Replace the path with the supplied path. The supplied path
 * may or may not end with a '/' character. Examples:
 *
 * <pre>
 * m_sCSID            p          resulting m_sCSID
 * ------------------ ---------- --------------------
 * d:/x/y/z/a/sman    c:/cal     c:/cal/sman
 * sman               c:/cal     c:/cal/sman
 * sman               c:/cal/    c:/cal/sman
 * </pre>
 *
 * @return 0  success
 */
nsresult nsCurlParser::SetPath(const JulianString& s)
{
  return SetPath(s.GetBuffer());
}

static char *gtpDecodedFileChars = ":\\";
static char *gtpEncodedFileChars = "|/";
#define NSCURLPARSER_URLFILECHARS 2
#define NSCURLPARSER_OSFILECHARS 1

void nsCurlParser::ConvertToURLFileChars(JulianString& s)
{
  PRInt32 i,j;

  for (j = 0; j < NSCURLPARSER_URLFILECHARS; j++)
  {
    while (-1 != (i = s.Find(gtpDecodedFileChars[j])))
      s.GetBuffer()[i] = gtpEncodedFileChars[j];
  }
}

void nsCurlParser::ConvertToFileChars(JulianString& s)
{
  PRInt32 i,j;

  for (j = 0; j < NSCURLPARSER_OSFILECHARS; j++)
  {
    while (-1 != (i = s.Find(gtpEncodedFileChars[j])))
      s.GetBuffer()[i] = gtpDecodedFileChars[j];
  }
}

/**
 * Clean up the CSID portion of this curl. The CSID portion is 
 * presumed to be a file name. Convert DOS/Windows file characters
 * into cross-platform or URL-friendly characters.
 * @return 0 on success
 */
nsresult nsCurlParser::ResolveFileURL()
{
  nsCurlParser t;
  JulianString s;
  s = GetCSID();
  ConvertToURLFileChars(s);
  t.SetCSID( s );
  t.SetProtocol(eFILE);
  char *p = t.ToLocalFile();
  SetCSID(p);
  PR_Free(p);
  return 0;
}

/**
 * Set the extra info
 * @return 0  success
 */
nsresult nsCurlParser::SetExtra(const JulianString& s)
{
  m_sExtra = s;
  m_bAssemblyNeeded = PR_TRUE;
  return 0;
}

/**
 * Set the extra info
 * @return 0  success
 */
nsresult nsCurlParser::SetExtra(const char* s)
{
  if ( m_sExtra.CompareTo(s,PR_TRUE))
  {
    m_sExtra = s;
    m_bAssemblyNeeded = PR_TRUE;
  }
  return 0;
}

/**
 * Get the Extra info
 * @result an JulianString containing the current extra info.
 */
JulianString nsCurlParser::GetExtra()
{
  return m_sExtra;
}

/*
 * Assumes full path passed in.  This is to differentiate from
 * file: URL's
 */

PRBool nsCurlParser::IsLocalFile()
{
  PRBool bLocal = PR_FALSE;

  char * pszFile = (char *) m_sCurl.GetBuffer();

  if (pszFile != nsnull)
  {

/// XP_MAC?
#ifdef XP_UNIX
    if (PL_strlen(pszFile) > 0)
    {
      if (pszFile[0] == '/')
        bLocal = PR_TRUE;
    }
#else
    if (PL_strlen(pszFile) > 2)
    {
      if ((pszFile[1] == ':') && (pszFile[2] == '\\'))
        bLocal = PR_TRUE;
    }
#endif

  }

  return bLocal;
}

/*
 * this method converts a "file://" or "resource://" URL
 * to a platform specific file handle.  It is the caller's
 * responsibility to free ther returned string using 
 * PR_Free().
 *
 * This code was lifted partly from Live3D and Netlib (nsURL.cpp).
 * May the Lord above have mercy on my soul.....
 *
 */

/**
 * additional notes from sman...
 * The return value has been PR_Malloc'd. Be sure to use PR_Free
 * when you finish with it.
 */

char * nsCurlParser::ToLocalFile()
{
  char * pszFile = nsnull;

  if (m_bAssemblyNeeded)
      Assemble();
  char * pszURL = (char *) m_sCurl.GetBuffer();

  /*
   * Check to see it is a local file already
   */
  if (IsLocalFile() == PR_TRUE)
    return pszURL;

  if (PL_strncasecmp(pszURL, "resource://",11) == 0)
  {
    pszFile = ResourceToFile();

  } else {

    pszFile = (char *)PR_Malloc(PL_strlen(pszURL));

    pszFile[0] = '\0' ;

    if (PL_strncasecmp(pszURL, "file://", 7) == 0 || PL_strncasecmp(pszURL, "capi://", 7) == 0)
    {
      char *pSrc = pszURL + 7 ;

      if (PL_strncasecmp(pSrc, "//", 2) == 0)              // file:////<hostname>
      {
        PL_strcpy(pszFile, pSrc) ;
      }
      else
      {
        if (PL_strncasecmp(pSrc, "localhost/", 10) == 0)      // file://localhost/
        {
          pSrc += 10 ;  // Go past the '/' after "localhost"
        }
        else
        {
          if (*pSrc == '/')                           // file:///
            pSrc++ ;
        }

        if (*(pSrc+1) == '|') // We got a drive letter
        {
          PL_strcpy(pszFile, pSrc) ;
          pszFile[1] = ':' ;
        }
        else
        {
  #ifdef XP_UNIX
          pszFile[0] = '/' ;
          PL_strcpy(pszFile+1, pSrc) ;
  #else
          pszFile[0] = 'C' ;
          pszFile[1] = ':' ;
          pszFile[2] = '/' ;
          PL_strcpy(pszFile+3, pSrc) ;
  #endif
        }
      }
  #ifndef XP_UNIX
      char *pDest = pszFile ;
      while(*pDest != '\0')              // translate '/' to '\' -> PC stuff
      {
          if (*pDest == '/')
              *pDest = '\\' ;
          pDest++ ;
      }
  #endif
    }
  }

  return pszFile;
}

char * nsCurlParser::ResourceToFile()
{

  char * aResourceFileName = (char *) m_sCurl.GetBuffer();
  // XXX For now, resources are not in jar files 
  // Find base path name to the resource file
  char* resourceBase;

#ifdef XP_PC
  char * cp;
  // XXX For now, all resources are relative to the .exe file
  resourceBase = (char *)PR_Malloc(_MAX_PATH);;
  DWORD mfnLen = GetModuleFileName(NULL, resourceBase, _MAX_PATH);
  // Truncate the executable name from the rest of the path...
  cp = strrchr(resourceBase, '\\');
  if (nsnull != cp) {
    *cp = '\0';
  }
  // Change the first ':' into a '|'
  cp = PL_strchr(resourceBase, ':');
  if (nsnull != cp) {
      *cp = '|';
  }
#endif

#ifdef XP_UNIX
  // XXX For now, all resources are relative to the current working directory

    FILE *pp;

#define MAXPATHLEN 2000

    resourceBase = (char *)PR_Malloc(MAXPATHLEN);;

    if (!(pp = popen("pwd", "r"))) {
      printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 1\n");
      return(nsnull);
    }
    else {
      if (fgets(resourceBase, MAXPATHLEN, pp)) {
        printf("[%s] %d\n", resourceBase, PL_strlen(resourceBase));
        resourceBase[PL_strlen(resourceBase)-1] = 0;
      }
      else {
       printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 2\n");
       return(nsnull);
      }
   }

   printf("RESOURCE name %s\n", resourceBase);
#endif

  // Join base path to resource name
  if (aResourceFileName[0] == '/') {
    aResourceFileName++;
  }
  PRInt32 baseLen = PL_strlen(resourceBase);
  PRInt32 resLen = PL_strlen(aResourceFileName);
  PRInt32 totalLen = 8 + baseLen + 1 + resLen + 1;
  char* fileName = (char *)PR_Malloc(totalLen);
  PR_snprintf(fileName, totalLen, "file:///%s/%s", resourceBase, aResourceFileName+11);

#ifdef XP_PC
  // Change any backslashes into foreward slashes...
  while ((cp = PL_strchr(fileName, '\\')) != 0) {
    *cp = '/';
    cp++;
  }
#endif

  PR_Free(resourceBase);
  
  JulianString oldString = m_sCurl ;
  m_sCurl = fileName ;

  char *localFile = ToLocalFile();

  m_sCurl = oldString;

  PR_Free(fileName);

  return localFile;
}


#define MAX_URL 1024

char * nsCurlParser::LocalFileToURL()
{
  char * pszFile = (char *) m_sCurl.GetBuffer();
  
  char * pszURL = (char *)PR_Malloc(MAX_URL);

  pszURL[0] = '\0';

#ifdef XP_PC

  if (IsLocalFile() == PR_TRUE)
  {

    char    szDrive[5], szDir[1024], szFile[255], szExt[255] ;
    
    PL_strcpy(pszURL, "file:///") ;

    _splitpath(pszFile, szDrive, szDir, szFile, szExt) ;
           
    if (szDrive[0] != '\0')
    {
        PL_strcat(pszURL, szDrive) ;

        pszURL[PL_strlen(pszURL)-1] = '|' ;
    }

    char *pDest = szDir ;

    while(*pDest != '\0')              // translate '\' to '/' -> PC stuff
    {
        if (*pDest == '\\')
            *pDest = '/' ;      
        pDest++ ;
    }                  
    
    PL_strcat(pszURL, szDir) ;
    PL_strcat(pszURL, szFile) ;
    PL_strcat(pszURL, szExt) ;

  }
  else
  {
  // ***************************************************************
  // Need to handle file spec of "\\host\c\vrml\bobo.wrl"
  // and convert it to           "file:////host/c/vrml/bobo.wrl"

      if ((pszFile[0] == '\\') && (pszFile[1] == '\\'))
      {
          PL_strcpy(pszURL, "file://") ;
          PL_strcat(pszURL, pszFile) ;

          char *pDest = pszURL ;

          while(*pDest != '\0')              // translate '\' to '/' -> PC stuff
          {
              if (*pDest == '\\')
                  *pDest = '/' ;      
              pDest++ ;
          }                  
      }
      else
      {
          // TODO: Should check that pszFile is a valid HTTP here!
          PL_strcpy(pszURL, pszFile) ;
      }
  }

#endif

  //delete pszFile;

  return pszURL;
}
