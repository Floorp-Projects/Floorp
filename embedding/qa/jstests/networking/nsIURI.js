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
 *   Ashish Bhatt <ashishbhatt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


const IO_SERVICE_CTRID = 
	"@mozilla.org/network/io-service;1" ;
const nsIIOService = 
	Components.interfaces.nsIIOService ;

function URI(aSpec,aOriginCharset,aBaseURI)
{
    try {
	    netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
	    

	    ioservice = Components.classes[IO_SERVICE_CTRID].getService(nsIIOService);

	    
	    if(!ioservice)
	       this.exception = "Unable to get nsIIOService" ;
	          
	    this.uri = ioservice.newURI(aSpec,aOriginCharset,aBaseURI) ;

	    if (!this.uri)
	       this.exception = "Unable to get nsIURI" ;
	      
	   return this ;
     }
     catch(e){
	this.exception = e ;
     }
     
	 
   
}

URI.prototype.returnvalue = null;
URI.prototype.success = null;
URI.prototype.exception = null;
URI.prototype.uri = null ;



URI.prototype.scheme getter = function ()
{

 /**
  * The Scheme is the protocol to which this URI refers.  The scheme is
  * restricted to the US-ASCII charset per RFC2396.
  */
  
  try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.uri.scheme ;
	 
	  if (this.returnvalue)
	    this.success =  true ;
	  else
	    this.success =  false ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

}

URI.prototype.scheme setter = function (aScheme)
{

 /**
  * The Scheme is the protocol to which this URI refers.  The scheme is
  * restricted to the US-ASCII charset per RFC2396.
  */
  
  try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.uri.scheme  = aScheme ;
 
	  this.returnvalue =  this.uri.scheme
	  this.success =  true ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

}

URI.prototype.spec getter = function ()
{

 /**
  * Returns a string representation of the URI. Setting the spec causes
  * the new spec to be parsed, initializing the URI.
  *
  * Some characters may be escaped.
  */
   
   try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.uri.spec ;
	 
	  if (this.returnvalue)
	    this.success =  true ;
	  else
	    this.success =  false ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

}

URI.prototype.spec setter = function (aSpec)
{

 /**
  * Returns a string representation of the URI. Setting the spec causes
  * the new spec to be parsed, initializing the URI.
  *
  * Some characters may be escaped.
  */
   
   try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.uri.spec =  aSpec ; ;
	 this.returnvalue = this.uri.spec ;
	 
	 this.success =  true ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

}

URI.prototype.prePath getter = function ()
{

  /**
  * The prePath (eg. scheme://user:password@host:port) returns the string
  * before the path.  This is useful for authentication or managing sessions.
  *
  * Some characters may be escaped.
  */
   
   try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.uri.prePath ;
	 
	  if (this.returnvalue)
	    this.success =  true ;
	  else
	    this.success =  false ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

}


URI.prototype.userPass getter= function ()
{

 /**
  * The username:password (or username only if value doesn't contain a ':')
  *
  * Some characters may be escaped.
  */
    try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.uri.userPass ;
     this.success =  true ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }
 
}

URI.prototype.userPass setter= function (aUserPass)
{

 /**
  * The username:password (or username only if value doesn't contain a ':')
  *
  * Some characters may be escaped.
  */
    try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.uri.userPass = aUserPass ;
	 this.returnvalue = this.uri.userPass ;
     this.success =  true ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }
 
}

URI.prototype.username getter = function ()
{

 /**
  * The optional username and password, assuming the preHost consists of
  * username:password.
  *
  * Some characters may be escaped.
  *
  *  attribute AUTF8String username;
  */ 
  
   try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.uri.username ;
	 
	  if (this.returnvalue)
      this.success =  true ;
 }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

}

URI.prototype.username setter = function (aUserName)
{

 /**
  * The optional username and password, assuming the preHost consists of
  * username:password.
  *
  * Some characters may be escaped.
  *
  *  attribute AUTF8String username;
  */ 
  
   try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.uri.username = aUserName ;
	 this.returnvalue = this.uri.username ;
	 
	  if (this.returnvalue)
      this.success =  true ;
 }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

}

URI.prototype.password getter = function ()
{
 /**
  * The optional username and password, assuming the preHost consists of
  * username:password.
  *
  * Some characters may be escaped.
  *
  *  attribute AUTF8String password;
  */ 
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.password ;
 	 
 	  if (this.returnvalue)
      this.success =  true ;
  }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}

URI.prototype.password setter = function (aPassword)
{
 /**
  * The optional username and password, assuming the preHost consists of
  * username:password.
  *
  * Some characters may be escaped.
  *
  *  attribute AUTF8String password;
  */ 
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.uri.password = aPassword ;
 	 this.returnvalue = this.uri.password ;
 	 
 	  if (this.returnvalue)
      this.success =  true ;
  }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}

URI.prototype.hostPort getter= function ()
{
  /**
  * The host:port (or simply the host, if port == -1).
  *
  * Characters are NOT escaped.
  *
  *	attribute AUTF8String hostPort;
  */
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.hostPort ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}

URI.prototype.hostPort setter= function (aHostPort)
{
  /**
  * The host:port (or simply the host, if port == -1).
  *
  * Characters are NOT escaped.
  *
  *	attribute AUTF8String hostPort;
  */
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.uri.hostPort = aHostPort ;
 	 this.returnvalue = this.uri.hostPort ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}

URI.prototype.host getter = function ()
{
 /**
  * The host is the internet domain name to which this URI refers.  It could
  * be an IPv4 (or IPv6) address literal.  If supported, it could be a
  * non-ASCII internationalized domain name.
  *
  * Characters are NOT escaped.
  *
  *	attribute AUTF8String host;
  */
    try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.host ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}

URI.prototype.host setter = function (aHost)
{
 /**
  * The host is the internet domain name to which this URI refers.  It could
  * be an IPv4 (or IPv6) address literal.  If supported, it could be a
  * non-ASCII internationalized domain name.
  *
  * Characters are NOT escaped.
  *
  *	attribute AUTF8String host;
  */
    try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.uri.host = aHost;
 	 this.returnvalue = this.uri.host ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}


URI.prototype.port getter = function ()
{
 /**
  * A port value of -1 corresponds to the protocol's default port (eg. -1
  * implies port 80 for http URIs).
  *
  *	attribute long port;
  */
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.port ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}

URI.prototype.port setter = function (aPort)
{
 /**
  * A port value of -1 corresponds to the protocol's default port (eg. -1
  * implies port 80 for http URIs).
  *
  *	attribute long port;
  */
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.uri.port = aPort ;
 	 this.returnvalue = this.uri.port ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}


URI.prototype.path getter = function ()
{
 /**
  * The path, typically including at least a leading '/' (but may also be
  * empty, depending on the protocol).
  *
  * Some characters may be escaped.
  *
  *	attribute AUTF8String path;
  */
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.path ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

 
}

URI.prototype.path setter = function (aPath)
{
 /**
  * The path, typically including at least a leading '/' (but may also be
  * empty, depending on the protocol).
  *
  * Some characters may be escaped.
  *
  *	attribute AUTF8String path;
  */
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.uri.path  = aPath ;
 	 this.returnvalue = this.uri.path ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

 
}



URI.prototype.equals = function (aURI)
{
 /**
  * URI equivalence test (not a strict string comparison).
  * eg. http://foo.com:80/ == http://foo.com/
  *
  *	boolean equals(in nsIURI other);
  */
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.equals(aURI);
 	 this.success =  true ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}



URI.prototype.schemeIs = function (aScheme)
{
 /**
  * An optimization to do scheme checks without requiring the users of nsIURI
  * to GetScheme, thereby saving extra allocating and freeing. Returns true if
  * the schemes match (case ignored).
  *
  *	boolean schemeIs(in string scheme);
  */
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.schemeIs(aScheme)	 ;
 	 this.success =  true ;
 	 
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}



URI.prototype.clone = function ()

{
 /**
  * Clones the current URI.  For some protocols, this is more than just an
  * optimization.  For example, under MacOS, the spec of a file URL does not
  * necessarily uniquely identify a file since two volumes could share the
  * same name.
  *
  * nsIURI clone();
  */
  
    try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.clone() ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}



URI.prototype.resolve = function (aRelativePath)
{
 /**
  * This method resolves a relative string into an absolute URI string,
  * using this URI as the base. 
  *
  * NOTE: some implementations may have no concept of a relative URI.
  *
  * AUTF8String resolve(in AUTF8String relativePath);
  */
  
    try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.resolve(aRelativePath) ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}



URI.prototype.asciiSpec = function ()
{

 /**
  * The URI spec with an ASCII compatible encoding.  Host portion follows
  * the IDNA draft spec.  Other parts are URL-escaped per the rules of
  * RFC2396.  The result is strictly ASCII.
  *
  * readonly attribute ACString asciiSpec;   
  */
  
 try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.uri.asciiSpec ;
	 
	  if (this.returnvalue)
	    this.success =  true ;
	  else
	    this.success =  false ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

 }
 
 
 
 
URI.prototype.asciiHost = function ()
{
 /**
  * The URI host with an ASCII compatible encoding.  Follows the IDNA
  * draft spec for converting internationalized domain names (UTF-8) to
  * ASCII for compatibility with existing internet infrasture.
  *
  *	readonly attribute ACString asciiHost;
  */
  
  try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.asciiHost ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

}




URI.prototype.originCharset = function ()
{
 /**
  * The charset of the document from which this URI originated.  An empty
  * value implies UTF-8.
  *
  * If this value is something other than UTF-8 then the URI components
  * (e.g., spec, prePath, username, etc.) will all be fully URL-escaped.
  * Otherwise, the URI components may contain unescaped multibyte UTF-8
  * characters.
  *
  * readonly attribute ACString originCharset;
  */
    try {
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
 	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
 
 	 this.returnvalue = this.uri.originCharset ;
 	 
 	  if (this.returnvalue)
 	    this.success =  true ;
 	  else
 	    this.success =  false ;
   }
   catch(e){
     this.success =  false ;
     this.exception =  e ;
   }

 }
 