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

function FileProtocolHandler(aScheme)
{
    try {
	    netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
	    

	    ioservice = Components.classes[IO_SERVICE_CTRID].getService(nsIIOService);

	    
	    if(!ioservice)
	       this.exception = "Unable to get nsIIOService" ;
	          
	    this.protocolhandler = ioservice.getProtocolHandler(aScheme) ;

	    if (!this.protocolhandler)
	       this.exception = "Unable to get nsIProtocolHandler" ;
	      
	   return this ;
     }
     catch(e){
	this.exception = e ;
     }
     
	 
   
}

FileProtocolHandler.prototype.returnvalue = null;
FileProtocolHandler.prototype.success = null;
FileProtocolHandler.prototype.exception = null;
FileProtocolHandler.prototype.protocolhandler = null ;



FileProtocolHandler.prototype.scheme = function ()
{

   /**
   * The scheme of this protocol (e.g., "file").
   */
  
  try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.protocolhandler.scheme ;
	 
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

FileProtocolHandler.prototype.defaultPort = function ()
{

   /** 
    * The default port is the port that this protocol normally uses.
    * If a port does not make sense for the protocol (e.g., "about:")
    * then -1 will be returned.
   */
   
   try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.protocolhandler.defaultPort ;
	 
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

FileProtocolHandler.prototype.protocolFlags = function ()
{

   /**
    * Returns the protocol specific flags (see flag definitions below).
    */
   
   try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.protocolhandler.protocolFlags ;
	 
	  if (this.returnvalue>=0)
	    this.success =  true ;
	  else
	    this.success =  false ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

}


FileProtocolHandler.prototype.newURI =  function(aSpec,aOriginCharset,aBaseURI)
{

  /**
   * Makes a URI object that is suitable for loading by this protocol,
   * where the URI string is given as an UTF-8 string.  The caller may
   * provide the charset from which the URI string originated, so that
   * the URI string can be translated back to that charset (if necessary)
   * before communicating with, for example, the origin server of the URI
   * string.  (Many servers do not support UTF-8 IRIs at the present time,
   * so we must be careful about tracking the native charset of the origin
   * server.)
   *
   * @param aSpec          - the URI string in UTF-8 encoding. depending
   *                         on the protocol implementation, unicode character
   *                         sequences may or may not be %xx escaped.
   * @param aOriginCharset - the charset of the document from which this URI
   *                         string originated.  this corresponds to the
   *                         charset that should be used when communicating
   *                         this URI to an origin server, for example.  if
   *                         null, then UTF-8 encoding is assumed (i.e.,
   *                         no charset transformation from aSpec).
   * @param aBaseURI       - if null, aSpec must specify an absolute URI.
   *                         otherwise, aSpec may be resolved relative
   *                         to aBaseURI, depending on the protocol. 
   *                         If the protocol has no concept of relative 
   *                         URI aBaseURI will simply be ignored.
   */
   
   
  try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.protocolhandler.newURI(aSpec,aOriginCharset,aBaseURI)
	 
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

FileProtocolHandler.prototype.newChannel = function(aURI)
{
	 /**
	  * Creates a channel for a given URI.
	  *
	  * @param aURI nsIURI from which to make a channel
	  * @return reference to the new nsIChannel object
	  *
	  * nsIChannel newChannelFromURI(in nsIURI aURI);
	  */

	  try{

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

		this.returnvalue  = this.protocolhandler.newChannel(aURI) ;
		
		if (this.returnvalue)
		  this.success = true
		else
		  this.success = false;
	 }
	 catch(e){   
		this.success = false;
		this.exception = e ;
	  }

}

FileProtocolHandler.prototype.allowPort= function(aPort, aScheme)
{
	 /**
	  * Checks if a port number is banned. This involves consulting a list of
	  * unsafe ports, corresponding to network services that may be easily
	  * exploitable. If the given port is considered unsafe, then the protocol
	  * handler (corresponding to aScheme) will be asked whether it wishes to
	  * override the IO service's decision to block the port. This gives the
	  * protocol handler ultimate control over its own security policy while
	  * ensuring reasonable, default protection.
	  *
	  * @see nsIProtocolHandler::allowPort
	  *
	  * boolean allowPort(in long aPort, in string aScheme);
	  */


	  try{

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

		this.returnvalue  = this.protocolhandler.allowPort(aPort, aScheme) ;
		this.success = true
	 }
	 catch(e){   
		this.success = false;
		this.exception = e ;
	  }

}
