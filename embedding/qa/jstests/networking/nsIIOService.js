/* ***** BEGIN LICENSE BLOCK *****
 Version: MPL 1.1/GPL 2.0/LGPL 2.1

 The contents of this file are subject to the Mozilla Public License Version
 1.1 (the "License"); you may not use this file except in compliance with
 the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS IS" basis,
 WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 for the specific language governing rights and limitations under the
 License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is
 Netscape Communications Corporation.
 Portions created by the Initial Developer are Copyright (C) 1998
 the Initial Developer. All Rights Reserved.

 Contributor(s):
   Ashish Bhatt <ashishbhatt@netscape.com> Original Author

 Alternatively, the contents of this file may be used under the terms of
 either the GNU General Public License Version 2 or later (the "GPL"), or
 the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 in which case the provisions of the GPL or the LGPL are applicable instead
 of those above. If you wish to allow use of your version of this file only
 under the terms of either the GPL or the LGPL, and not to allow others to
 use your version of this file under the terms of the MPL, indicate your
 decision by deleting the provisions above and replace them with the notice
 and other provisions required by the GPL or the LGPL. If you do not delete
 the provisions above, a recipient may use your version of this file under
 the terms of any one of the MPL, the GPL or the LGPL.

 ***** END LICENSE BLOCK ***** */


const nsIIOService =
     Components.interfaces.nsIIOService
const IO_SERVICE_CTRID =
      "@mozilla.org/network/io-service;1" ;


function IOService()
{
   
   try{
   	netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
   	netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
   
       
       this.service  = Components.classes[IO_SERVICE_CTRID].getService(nsIIOService);
       if (!this.service)
       	   return null ;
       return this ;
   }
   catch(e)
   {
     this.exception = e ;
     return null;
   }
}


IOService.prototype.service =  null ;
IOService.prototype.success =  null ;
IOService.prototype.exception =  null ;
IOService.prototype.returnvalue =  null ;


IOService.prototype.getProtocolHandler = function(aScheme)
{

	 /**
	  * Returns a protocol handler for a given URI scheme.
	  *
	  * @param aScheme the URI scheme
	  * @return reference to corresponding nsIProtocolHandler
	  *
	  * nsIProtocolHandler getProtocolHandler(in string aScheme)
	  */

	  try{

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

		this.returnvalue  = this.service.getProtocolHandler(aScheme) ;
		
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

IOService.prototype.getProtocolFlags= function(aScheme)
{
	 /**
	  * Returns the protocol flags for a given scheme.
	  *
	  * @param aScheme the URI scheme
	  * @return value of corresponding nsIProtocolHandler::protocolFlags
	  *
	  * unsigned long getProtocolFlags(in string aScheme);
	  */


	  try{

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

		this.returnvalue  = this.service.getProtocolFlags(aScheme) ;
		if (this.returnvalue>=0)
		  this.success = true
		else
		  this.success = false;
	 }
	 catch(e){   
		this.success = false;
		this.exception = e ;
	  }
}

IOService.prototype.newURI =  function(aSpec,aOriginCharset,aBaseURI)
{
	 /**
	  * This method constructs a new URI by determining the scheme of the
	  * URI spec, and then delegating the construction of the URI to the
	  * protocol handler for that scheme. QueryInterface can be used on
	  * the resulting URI object to obtain a more specific type of URI.
	  *
	  * @see nsIProtocolHandler::newURI
	  *
	  * nsIURI newURI(in AUTF8String aSpec,
	  *			   in string aOriginCharset,
	  *			   in nsIURI aBaseURI);
	  */

	  try{

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

		this.returnvalue  = this.service.newURI(aSpec,aOriginCharset,aBaseURI) ;
		
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


IOService.prototype.newFileURI= function(file)
{
	 /**
	  * This method constructs a new URI from a nsIFile.
	  *
	  * @param aFile specifies the file path
	  * @return reference to a new nsIURI object
	  *
	  * nsIURI newFileURI(in nsIFile aFile);
	  */

	  try{

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

		this.returnvalue  = this.service.newFileURI(file) ;
		
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


IOService.prototype.newChannelFromURI = function(aURI)
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

		this.returnvalue  = this.service.newChannelFromURI(aURI) ;
		
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


IOService.prototype.newChannel = function(aSpec,aOriginCharset,aBaseURI)
{
	 /**
	  * Equivalent to newChannelFromURI(newURI(...))
	  *
	  * nsIChannel newChannel(in AUTF8String aSpec,
	  *					   in string aOriginCharset,
	  *					   in nsIURI aBaseURI);
	  */

	  try{

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

		this.returnvalue  = this.service.newChannel(aSpec,aOriginCharset,aBaseURI) ;
		
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

IOService.prototype.Getoffline = function()
{

	 /**
	  * Returns true if networking is in "offline" mode. When in offline mode,
	  * attempts to access the network will fail (although this is not
	  * necessarily corrolated with whether there is actually a network
	  * available -- that's hard to detect without causing the dialer to
	  * come up).
	  *
	  *	attribute boolean offline;
	  */

	  try{

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

		this.returnvalue  = this.service.offline ;
		this.success = true;
	 }
	 catch(e){   
		this.success = false;
		this.exception = e ;
	  }

}

IOService.prototype. Setoffline = function(aOffline)
{
	 /**
	  * Returns true if networking is in "offline" mode. When in offline mode,
	  * attempts to access the network will fail (although this is not
	  * necessarily corrolated with whether there is actually a network
	  * available -- that's hard to detect without causing the dialer to
	  * come up).
	  *
	  *	attribute boolean offline;
	  */
	  
	  try{

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
		
		this.service.offline  = aOffline ;
		this.returnvalue  = this.service.offline 
		this.success = true;
	 }
	 catch(e){   
		this.success = false;
		this.exception = e ;
	  }

}

IOService.prototype.allowPort= function(aPort, aScheme)
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

		this.returnvalue  = this.service.allowPort(aPort, aScheme) ;
		
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

IOService.prototype.extractScheme = function(urlString)
{
	 /**
	  * Utility to extract the scheme from a URL string, consistently and
	  * according to spec (see RFC 2396).
	  *
	  * NOTE: Most URL parsing is done via nsIURI, and in fact the scheme
	  * can also be extracted from a URL string via nsIURI.  This method
	  * is provided purely as an optimization.
	  *
	  * @param aSpec the URL string to parse
	  * @return URL scheme
	  *
	  * @throws NS_ERROR_MALFORMED_URI if URL string is not of the right form.
	  *
	  * ACString extractScheme(in AUTF8String urlString);
	  */

	  try
	  {

		netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
		netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

		this.returnvalue  = this.service.extractScheme(urlString) ;
		
		if (this.returnvalue)
		  this.success = true
		else
		  this.success = false;
	 }
	 catch(e)
	 {   
		this.success = false;
		this.exception = e ;
	 }

}
